/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const {l10n} = require("devtools/shared/inspector/css-logic");
const {ELEMENT_STYLE} = require("devtools/shared/specs/styles");
const Rule = require("devtools/client/inspector/rules/models/rule");
const {
  InplaceEditor,
  editableField,
  editableItem
} = require("devtools/client/shared/inplace-editor");
const TextPropertyEditor =
  require("devtools/client/inspector/rules/views/text-property-editor");
const {
  createChild,
  blurOnMultipleProperties,
  promiseWarn
} = require("devtools/client/inspector/shared/utils");
const {
  parseNamedDeclarations,
  parsePseudoClassesAndAttributes,
  SELECTOR_ATTRIBUTE,
  SELECTOR_ELEMENT,
  SELECTOR_PSEUDO_CLASS
} = require("devtools/shared/css/parsing-utils");
const promise = require("promise");
const Services = require("Services");
const EventEmitter = require("devtools/shared/event-emitter");
const {Tools} = require("devtools/client/definitions");
const {gDevTools} = require("devtools/client/framework/devtools");
const CssLogic = require("devtools/shared/inspector/css-logic");

const STYLE_INSPECTOR_PROPERTIES = "devtools/shared/locales/styleinspector.properties";
const {LocalizationHelper} = require("devtools/shared/l10n");
const STYLE_INSPECTOR_L10N = new LocalizationHelper(STYLE_INSPECTOR_PROPERTIES);

/**
 * RuleEditor is responsible for the following:
 *   Owns a Rule object and creates a list of TextPropertyEditors
 *     for its TextProperties.
 *   Manages creation of new text properties.
 *
 * @param {CssRuleView} ruleView
 *        The CssRuleView containg the document holding this rule editor.
 * @param {Rule} rule
 *        The Rule object we're editing.
 */
function RuleEditor(ruleView, rule) {
  EventEmitter.decorate(this);

  this.ruleView = ruleView;
  this.doc = this.ruleView.styleDocument;
  this.toolbox = this.ruleView.inspector.toolbox;
  this.telemetry = this.toolbox.telemetry;
  this.rule = rule;

  this.isEditable = !rule.isSystem;
  // Flag that blocks updates of the selector and properties when it is
  // being edited
  this.isEditing = false;

  this._onNewProperty = this._onNewProperty.bind(this);
  this._newPropertyDestroy = this._newPropertyDestroy.bind(this);
  this._onSelectorDone = this._onSelectorDone.bind(this);
  this._locationChanged = this._locationChanged.bind(this);
  this.updateSourceLink = this.updateSourceLink.bind(this);
  this._onToolChanged = this._onToolChanged.bind(this);
  this._updateLocation = this._updateLocation.bind(this);
  this._onSourceClick = this._onSourceClick.bind(this);

  this.rule.domRule.on("location-changed", this._locationChanged);
  this.toolbox.on("tool-registered", this._onToolChanged);
  this.toolbox.on("tool-unregistered", this._onToolChanged);

  this._create();
}

RuleEditor.prototype = {
  destroy: function() {
    this.rule.domRule.off("location-changed");
    this.toolbox.off("tool-registered", this._onToolChanged);
    this.toolbox.off("tool-unregistered", this._onToolChanged);

    let url = null;
    if (this.rule.sheet) {
      url = this.rule.sheet.href || this.rule.sheet.nodeHref;
    }
    if (url && !this.rule.isSystem && this.rule.domRule.type !== ELEMENT_STYLE) {
      // Only get the original source link if the rule isn't a system
      // rule and if it isn't an inline rule.
      const sourceLine = this.rule.ruleLine;
      const sourceColumn = this.rule.ruleColumn;

      if (this._sourceMapURLService) {
        this._sourceMapURLService.unsubscribe(url, sourceLine, sourceColumn,
          this._updateLocation);
      }
    }
  },

  get sourceMapURLService() {
    if (!this._sourceMapURLService) {
      // sourceMapURLService is a lazy getter in the toolbox.
      this._sourceMapURLService = this.toolbox.sourceMapURLService;
    }

    return this._sourceMapURLService;
  },

  get isSelectorEditable() {
    const trait = this.isEditable &&
      this.ruleView.inspector.target.client.traits.selectorEditable &&
      this.rule.domRule.type !== ELEMENT_STYLE &&
      this.rule.domRule.type !== CSSRule.KEYFRAME_RULE;

    // Do not allow editing anonymousselectors until we can
    // detect mutations on  pseudo elements in Bug 1034110.
    return trait && !this.rule.elementStyle.element.isAnonymous;
  },

  _create: function() {
    this.element = this.doc.createElement("div");
    this.element.className = "ruleview-rule devtools-monospace";
    this.element.setAttribute("uneditable", !this.isEditable);
    this.element.setAttribute("unmatched", this.rule.isUnmatched);
    this.element._ruleEditor = this;

    // Give a relative position for the inplace editor's measurement
    // span to be placed absolutely against.
    this.element.style.position = "relative";

    // Add the source link.
    this.source = createChild(this.element, "div", {
      class: "ruleview-rule-source theme-link"
    });
    this.source.addEventListener("click", this._onSourceClick);

    const sourceLabel = this.doc.createElement("span");
    sourceLabel.classList.add("ruleview-rule-source-label");
    this.source.appendChild(sourceLabel);

    this.updateSourceLink();

    const code = createChild(this.element, "div", {
      class: "ruleview-code"
    });

    const header = createChild(code, "div", {});

    this.selectorText = createChild(header, "span", {
      class: "ruleview-selectorcontainer",
      tabindex: this.isSelectorEditable ? "0" : "-1",
    });

    if (this.isSelectorEditable) {
      this.selectorText.addEventListener("click", event => {
        // Clicks within the selector shouldn't propagate any further.
        event.stopPropagation();
      });

      editableField({
        element: this.selectorText,
        done: this._onSelectorDone,
        cssProperties: this.rule.cssProperties,
      });
    }

    if (this.rule.domRule.type !== CSSRule.KEYFRAME_RULE) {
      (async function() {
        let selector;

        if (this.rule.domRule.selectors) {
          // This is a "normal" rule with a selector.
          selector = this.rule.domRule.selectors.join(", ");
        } else if (this.rule.inherited) {
          // This is an inline style from an inherited rule. Need to resolve the unique
          // selector from the node which rule this is inherited from.
          selector = await this.rule.inherited.getUniqueSelector();
        } else {
          // This is an inline style from the current node.
          selector = this.ruleView.inspector.selectionCssSelector;
        }

        const isHighlighted = this.ruleView._highlighters &&
          this.ruleView.highlighters.selectorHighlighterShown === selector;
        const selectorHighlighter = createChild(header, "span", {
          class: "ruleview-selectorhighlighter" +
                 (isHighlighted ? " highlighted" : ""),
          title: l10n("rule.selectorHighlighter.tooltip")
        });
        selectorHighlighter.addEventListener("click", () => {
          this.ruleView.toggleSelectorHighlighter(selectorHighlighter, selector);
        });

        this.uniqueSelector = selector;
        this.emit("selector-icon-created");
      }.bind(this))().catch(error => {
        console.error("Exception while getting unique selector", error);
      });
    }

    this.openBrace = createChild(header, "span", {
      class: "ruleview-ruleopen",
      textContent: " {"
    });

    this.propertyList = createChild(code, "ul", {
      class: "ruleview-propertylist"
    });

    this.populate();

    this.closeBrace = createChild(code, "div", {
      class: "ruleview-ruleclose",
      tabindex: this.isEditable ? "0" : "-1",
      textContent: "}"
    });

    if (this.isEditable) {
      // A newProperty editor should only be created when no editor was
      // previously displayed. Since the editors are cleared on blur,
      // check this.ruleview.isEditing on mousedown
      this._ruleViewIsEditing = false;

      code.addEventListener("mousedown", () => {
        this._ruleViewIsEditing = this.ruleView.isEditing;
      });

      code.addEventListener("click", () => {
        const selection = this.doc.defaultView.getSelection();
        if (selection.isCollapsed && !this._ruleViewIsEditing) {
          this.newProperty();
        }
        // Cleanup the _ruleViewIsEditing flag
        this._ruleViewIsEditing = false;
      });

      this.element.addEventListener("mousedown", () => {
        this.doc.defaultView.focus();
      });

      // Create a property editor when the close brace is clicked.
      editableItem({ element: this.closeBrace }, () => {
        this.newProperty();
      });
    }
  },

  /**
   * Called when a tool is registered or unregistered.
   */
  _onToolChanged: function() {
    // When the source editor is registered, update the source links
    // to be clickable; and if it is unregistered, update the links to
    // be unclickable.  However, some links are never clickable, so
    // filter those out first.
    if (this.source.getAttribute("unselectable") === "permanent") {
      // Nothing.
    } else if (this.toolbox.isToolRegistered("styleeditor")) {
      this.source.removeAttribute("unselectable");
    } else {
      this.source.setAttribute("unselectable", "true");
    }
  },

  /**
   * Event handler called when a property changes on the
   * StyleRuleActor.
   */
  _locationChanged: function() {
    this.updateSourceLink();
  },

  _onSourceClick: function() {
    if (this.source.hasAttribute("unselectable") || !this._currentLocation) {
      return;
    }

    const target = this.ruleView.inspector.target;
    if (Tools.styleEditor.isTargetSupported(target)) {
      gDevTools.showToolbox(target, "styleeditor").then(toolbox => {
        const {url, line, column} = this._currentLocation;
        toolbox.getCurrentPanel().selectStyleSheet(url, line, column);
      });
    }
  },

  /**
   * Update the text of the source link to reflect whether we're showing
   * original sources or not.  This is a callback for
   * SourceMapURLService.subscribe, which see.
   *
   * @param {Boolean} enabled
   *        True if the passed-in location should be used; this means
   *        that source mapping is in use and the remaining arguments
   *        are the original location.  False if the already-known
   *        (stored) location should be used.
   * @param {String} url
   *        The original URL
   * @param {Number} line
   *        The original line number
   * @param {number} column
   *        The original column number
   */
  _updateLocation: function(enabled, url, line, column) {
    let displayURL = url;
    if (!enabled) {
      url = null;
      displayURL = null;
      if (this.rule.sheet) {
        url = this.rule.sheet.href || this.rule.sheet.nodeHref;
        displayURL = this.rule.sheet.href;
      }
      line = this.rule.ruleLine;
      column = this.rule.ruleColumn;
    }

    this._currentLocation = {
      url,
      line,
      column
    };

    let sourceTextContent = CssLogic.shortSource({href: displayURL});
    let title = displayURL ? displayURL : sourceTextContent;
    if (line > 0) {
      sourceTextContent += ":" + line;
      title += ":" + line;
    }
    if (this.rule.mediaText) {
      sourceTextContent += " @" + this.rule.mediaText;
      title += " @" + this.rule.mediaText;
    }

    const sourceLabel = this.element.querySelector(".ruleview-rule-source-label");
    sourceLabel.setAttribute("title", title);
    sourceLabel.textContent = sourceTextContent;
  },

  updateSourceLink: function() {
    if (this.rule.isSystem) {
      const sourceLabel = this.element.querySelector(".ruleview-rule-source-label");
      const title = this.rule.title;
      const sourceHref = (this.rule.sheet && this.rule.sheet.href) ?
          this.rule.sheet.href : title;

      const uaLabel = STYLE_INSPECTOR_L10N.getStr("rule.userAgentStyles");
      sourceLabel.textContent = uaLabel + " " + title;

      // Special case about:PreferenceStyleSheet, as it is generated on the
      // fly and the URI is not registered with the about: handler.
      // https://bugzilla.mozilla.org/show_bug.cgi?id=935803#c37
      if (sourceHref === "about:PreferenceStyleSheet") {
        this.source.setAttribute("unselectable", "permanent");
        sourceLabel.textContent = uaLabel;
        sourceLabel.removeAttribute("title");
      }
    } else {
      this._updateLocation(false);
    }

    let url = null;
    if (this.rule.sheet) {
      url = this.rule.sheet.href || this.rule.sheet.nodeHref;
    }
    if (url && !this.rule.isSystem && this.rule.domRule.type !== ELEMENT_STYLE) {
      // Only get the original source link if the rule isn't a system
      // rule and if it isn't an inline rule.
      const sourceLine = this.rule.ruleLine;
      const sourceColumn = this.rule.ruleColumn;
      this.sourceMapURLService.subscribe(url, sourceLine, sourceColumn,
                                         this._updateLocation);
      // Set "unselectable" appropriately.
      this._onToolChanged();
    } else if (this.rule.domRule.type === ELEMENT_STYLE) {
      this.source.setAttribute("unselectable", "permanent");
    } else {
      // Set "unselectable" appropriately.
      this._onToolChanged();
    }

    promise.resolve().then(() => {
      this.emit("source-link-updated");
    });
  },

  /**
   * Update the rule editor with the contents of the rule.
   */
  populate: function() {
    // Clear out existing viewers.
    while (this.selectorText.hasChildNodes()) {
      this.selectorText.removeChild(this.selectorText.lastChild);
    }

    // If selector text comes from a css rule, highlight selectors that
    // actually match.  For custom selector text (such as for the 'element'
    // style, just show the text directly.
    if (this.rule.domRule.type === ELEMENT_STYLE) {
      this.selectorText.textContent = this.rule.selectorText;
    } else if (this.rule.domRule.type === CSSRule.KEYFRAME_RULE) {
      this.selectorText.textContent = this.rule.domRule.keyText;
    } else {
      this.rule.domRule.selectors.forEach((selector, i) => {
        if (i !== 0) {
          createChild(this.selectorText, "span", {
            class: "ruleview-selector-separator",
            textContent: ", "
          });
        }

        const containerClass =
          (this.rule.matchedSelectors.indexOf(selector) > -1) ?
          "ruleview-selector-matched" : "ruleview-selector-unmatched";
        const selectorContainer = createChild(this.selectorText, "span", {
          class: containerClass
        });

        const parsedSelector = parsePseudoClassesAndAttributes(selector);

        for (const selectorText of parsedSelector) {
          let selectorClass = "";

          switch (selectorText.type) {
            case SELECTOR_ATTRIBUTE:
              selectorClass = "ruleview-selector-attribute";
              break;
            case SELECTOR_ELEMENT:
              selectorClass = "ruleview-selector";
              break;
            case SELECTOR_PSEUDO_CLASS:
              selectorClass = [":active", ":focus", ":hover"].some(
                  pseudo => selectorText.value === pseudo) ?
                "ruleview-selector-pseudo-class-lock" :
                "ruleview-selector-pseudo-class";
              break;
            default:
              break;
          }

          createChild(selectorContainer, "span", {
            textContent: selectorText.value,
            class: selectorClass
          });
        }
      });
    }

    for (const prop of this.rule.textProps) {
      if (!prop.editor && !prop.invisible) {
        const editor = new TextPropertyEditor(this, prop);
        this.propertyList.appendChild(editor.element);
      }
    }
  },

  /**
   * Programatically add a new property to the rule.
   *
   * @param {String} name
   *        Property name.
   * @param {String} value
   *        Property value.
   * @param {String} priority
   *        Property priority.
   * @param {Boolean} enabled
   *        True if the property should be enabled.
   * @param {TextProperty} siblingProp
   *        Optional, property next to which the new property will be added.
   * @return {TextProperty}
   *        The new property
   */
  addProperty: function(name, value, priority, enabled, siblingProp) {
    const prop = this.rule.createProperty(name, value, priority, enabled,
      siblingProp);
    const index = this.rule.textProps.indexOf(prop);
    const editor = new TextPropertyEditor(this, prop);

    // Insert this node before the DOM node that is currently at its new index
    // in the property list.  There is currently one less node in the DOM than
    // in the property list, so this causes it to appear after siblingProp.
    // If there is no node at its index, as is the case where this is the last
    // node being inserted, then this behaves as appendChild.
    this.propertyList.insertBefore(editor.element,
      this.propertyList.children[index]);

    return prop;
  },

  /**
   * Programatically add a list of new properties to the rule.  Focus the UI
   * to the proper location after adding (either focus the value on the
   * last property if it is empty, or create a new property and focus it).
   *
   * @param {Array} properties
   *        Array of properties, which are objects with this signature:
   *        {
   *          name: {string},
   *          value: {string},
   *          priority: {string}
   *        }
   * @param {TextProperty} siblingProp
   *        Optional, the property next to which all new props should be added.
   */
  addProperties: function(properties, siblingProp) {
    if (!properties || !properties.length) {
      return;
    }

    let lastProp = siblingProp;
    for (const p of properties) {
      const isCommented = Boolean(p.commentOffsets);
      const enabled = !isCommented;
      lastProp = this.addProperty(p.name, p.value, p.priority, enabled,
        lastProp);
    }

    // Either focus on the last value if incomplete, or start a new one.
    if (lastProp && lastProp.value.trim() === "") {
      lastProp.editor.valueSpan.click();
    } else {
      this.newProperty();
    }
  },

  /**
   * Create a text input for a property name.  If a non-empty property
   * name is given, we'll create a real TextProperty and add it to the
   * rule.
   */
  newProperty: function() {
    // If we're already creating a new property, ignore this.
    if (!this.closeBrace.hasAttribute("tabindex")) {
      return;
    }

    // While we're editing a new property, it doesn't make sense to
    // start a second new property editor, so disable focusing the
    // close brace for now.
    this.closeBrace.removeAttribute("tabindex");

    this.newPropItem = createChild(this.propertyList, "li", {
      class: "ruleview-property ruleview-newproperty",
    });

    this.newPropSpan = createChild(this.newPropItem, "span", {
      class: "ruleview-propertyname",
      tabindex: "0"
    });

    this.multipleAddedProperties = null;

    this.editor = new InplaceEditor({
      element: this.newPropSpan,
      done: this._onNewProperty,
      destroy: this._newPropertyDestroy,
      advanceChars: ":",
      contentType: InplaceEditor.CONTENT_TYPES.CSS_PROPERTY,
      popup: this.ruleView.popup,
      cssProperties: this.rule.cssProperties,
    });

    // Auto-close the input if multiple rules get pasted into new property.
    this.editor.input.addEventListener("paste",
      blurOnMultipleProperties(this.rule.cssProperties));
  },

  /**
   * Called when the new property input has been dismissed.
   *
   * @param {String} value
   *        The value in the editor.
   * @param {Boolean} commit
   *        True if the value should be committed.
   */
  _onNewProperty: function(value, commit) {
    if (!value || !commit) {
      return;
    }

    // parseDeclarations allows for name-less declarations, but in the present
    // case, we're creating a new declaration, it doesn't make sense to accept
    // these entries
    this.multipleAddedProperties =
      parseNamedDeclarations(this.rule.cssProperties.isKnown, value, true);

    // Blur the editor field now and deal with adding declarations later when
    // the field gets destroyed (see _newPropertyDestroy)
    this.editor.input.blur();

    this.telemetry.recordEvent("devtools.main", "edit_rule", "ruleview");
  },

  /**
   * Called when the new property editor is destroyed.
   * This is where the properties (type TextProperty) are actually being
   * added, since we want to wait until after the inplace editor `destroy`
   * event has been fired to keep consistent UI state.
   */
  _newPropertyDestroy: function() {
    // We're done, make the close brace focusable again.
    this.closeBrace.setAttribute("tabindex", "0");

    this.propertyList.removeChild(this.newPropItem);
    delete this.newPropItem;
    delete this.newPropSpan;

    // If properties were added, we want to focus the proper element.
    // If the last new property has no value, focus the value on it.
    // Otherwise, start a new property and focus that field.
    if (this.multipleAddedProperties && this.multipleAddedProperties.length) {
      this.addProperties(this.multipleAddedProperties);
    }
  },

  /**
   * Called when the selector's inplace editor is closed.
   * Ignores the change if the user pressed escape, otherwise
   * commits it.
   *
   * @param {String} value
   *        The value contained in the editor.
   * @param {Boolean} commit
   *        True if the change should be applied.
   * @param {Number} direction
   *        The move focus direction number.
   */
  async _onSelectorDone(value, commit, direction) {
    if (!commit || this.isEditing || value === "" ||
        value === this.rule.selectorText) {
      return;
    }

    const ruleView = this.ruleView;
    const elementStyle = ruleView._elementStyle;
    const element = elementStyle.element;
    const supportsUnmatchedRules =
      this.rule.domRule.supportsModifySelectorUnmatched;

    this.isEditing = true;

    try {
      const response = await this.rule.domRule.modifySelector(element, value);

      if (!supportsUnmatchedRules) {
        this.isEditing = false;

        if (response) {
          this.ruleView.refreshPanel();
        }
        return;
      }

      // We recompute the list of applied styles, because editing a
      // selector might cause this rule's position to change.
      const applied = await elementStyle.pageStyle.getApplied(element, {
        inherited: true,
        matchedSelectors: true,
        filter: elementStyle.showUserAgentStyles ? "ua" : undefined
      });

      this.isEditing = false;

      const {ruleProps, isMatching} = response;
      if (!ruleProps) {
        // Notify for changes, even when nothing changes,
        // just to allow tests being able to track end of this request.
        ruleView.emit("ruleview-invalid-selector");
        return;
      }

      ruleProps.isUnmatched = !isMatching;
      const newRule = new Rule(elementStyle, ruleProps);
      const editor = new RuleEditor(ruleView, newRule);
      const rules = elementStyle.rules;

      let newRuleIndex = applied.findIndex((r) => r.rule == ruleProps.rule);
      const oldIndex = rules.indexOf(this.rule);

      // If the selector no longer matches, then we leave the rule in
      // the same relative position.
      if (newRuleIndex === -1) {
        newRuleIndex = oldIndex;
      }

      // Remove the old rule and insert the new rule.
      rules.splice(oldIndex, 1);
      rules.splice(newRuleIndex, 0, newRule);
      elementStyle._changed();
      elementStyle.markOverriddenAll();

      // We install the new editor in place of the old -- you might
      // think we would replicate the list-modification logic above,
      // but that is complicated due to the way the UI installs
      // pseudo-element rules and the like.
      this.element.parentNode.replaceChild(editor.element, this.element);

      // Remove highlight for modified selector
      if (ruleView.highlighters.selectorHighlighterShown) {
        ruleView.toggleSelectorHighlighter(ruleView.lastSelectorIcon,
          ruleView.highlighters.selectorHighlighterShown);
      }

      editor._moveSelectorFocus(direction);
    } catch (err) {
      this.isEditing = false;
      promiseWarn(err);
    }
  },

  /**
   * Handle moving the focus change after a tab or return keypress in the
   * selector inplace editor.
   *
   * @param {Number} direction
   *        The move focus direction number.
   */
  _moveSelectorFocus: function(direction) {
    if (!direction || direction === Services.focus.MOVEFOCUS_BACKWARD) {
      return;
    }

    if (this.rule.textProps.length > 0) {
      this.rule.textProps[0].editor.nameSpan.click();
    } else {
      this.propertyList.click();
    }
  }
};

module.exports = RuleEditor;
