/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

/* eslint-disable mozilla/reject-some-requires */
const {XPCOMUtils} = require("resource://gre/modules/XPCOMUtils.jsm");
/* eslint-enable mozilla/reject-some-requires */
const {l10n} = require("devtools/shared/inspector/css-logic");
const {ELEMENT_STYLE} = require("devtools/shared/specs/styles");
const {PREF_ORIG_SOURCES} = require("devtools/client/styleeditor/utils");
const {Rule} = require("devtools/client/inspector/rules/models/rule");
const {InplaceEditor, editableField, editableItem} =
      require("devtools/client/shared/inplace-editor");
const {TextPropertyEditor} =
      require("devtools/client/inspector/rules/views/text-property-editor");
const {
  createChild,
  blurOnMultipleProperties,
  promiseWarn
} = require("devtools/client/inspector/shared/utils");
const {
  parseDeclarations,
  parsePseudoClassesAndAttributes,
  SELECTOR_ATTRIBUTE,
  SELECTOR_ELEMENT,
  SELECTOR_PSEUDO_CLASS
} = require("devtools/shared/css-parsing-utils");
const promise = require("promise");
const Services = require("Services");
const EventEmitter = require("devtools/shared/event-emitter");

XPCOMUtils.defineLazyGetter(this, "_strings", function () {
  return Services.strings.createBundle(
    "chrome://devtools-shared/locale/styleinspector.properties");
});

const HTML_NS = "http://www.w3.org/1999/xhtml";
const XUL_NS = "http://www.mozilla.org/keymaster/gatekeeper/there.is.only.xul";

/**
 * RuleEditor is responsible for the following:
 *   Owns a Rule object and creates a list of TextPropertyEditors
 *     for its TextProperties.
 *   Manages creation of new text properties.
 *
 * One step of a RuleEditor's instantiation is figuring out what's the original
 * source link to the parent stylesheet (in case of source maps). This step is
 * asynchronous and is triggered as soon as the RuleEditor is instantiated (see
 * updateSourceLink). If you need to know when the RuleEditor is done with this,
 * you need to listen to the source-link-updated event.
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

  this.rule.domRule.on("location-changed", this._locationChanged);
  this.toolbox.on("tool-registered", this.updateSourceLink);
  this.toolbox.on("tool-unregistered", this.updateSourceLink);

  this._create();
}

RuleEditor.prototype = {
  destroy: function () {
    this.rule.domRule.off("location-changed");
    this.toolbox.off("tool-registered", this.updateSourceLink);
    this.toolbox.off("tool-unregistered", this.updateSourceLink);
  },

  get isSelectorEditable() {
    let trait = this.isEditable &&
      this.toolbox.target.client.traits.selectorEditable &&
      this.rule.domRule.type !== ELEMENT_STYLE &&
      this.rule.domRule.type !== CSSRule.KEYFRAME_RULE;

    // Do not allow editing anonymousselectors until we can
    // detect mutations on  pseudo elements in Bug 1034110.
    return trait && !this.rule.elementStyle.element.isAnonymous;
  },

  _create: function () {
    this.element = this.doc.createElementNS(HTML_NS, "div");
    this.element.className = "ruleview-rule theme-separator";
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
    this.source.addEventListener("click", function () {
      if (this.source.hasAttribute("unselectable")) {
        return;
      }
      let rule = this.rule.domRule;
      this.ruleView.emit("ruleview-linked-clicked", rule);
    }.bind(this));
    let sourceLabel = this.doc.createElementNS(XUL_NS, "label");
    sourceLabel.setAttribute("crop", "center");
    sourceLabel.classList.add("ruleview-rule-source-label");
    this.source.appendChild(sourceLabel);

    this.updateSourceLink();

    let code = createChild(this.element, "div", {
      class: "ruleview-code"
    });

    let header = createChild(code, "div", {});

    this.selectorText = createChild(header, "span", {
      class: "ruleview-selectorcontainer theme-fg-color3",
      tabindex: this.isSelectorEditable ? "0" : "-1",
    });

    if (this.isSelectorEditable) {
      this.selectorText.addEventListener("click", event => {
        // Clicks within the selector shouldn't propagate any further.
        event.stopPropagation();
      }, false);

      editableField({
        element: this.selectorText,
        done: this._onSelectorDone,
      });
    }

    if (this.rule.domRule.type !== CSSRule.KEYFRAME_RULE &&
        this.rule.domRule.selectors) {
      let selector = this.rule.domRule.selectors.join(", ");

      let selectorHighlighter = createChild(header, "span", {
        class: "ruleview-selectorhighlighter" +
               (this.ruleView.highlightedSelector === selector ?
                " highlighted" : ""),
        title: l10n("rule.selectorHighlighter.tooltip")
      });
      selectorHighlighter.addEventListener("click", () => {
        this.ruleView.toggleSelectorHighlighter(selectorHighlighter, selector);
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
        let selection = this.doc.defaultView.getSelection();
        if (selection.isCollapsed && !this._ruleViewIsEditing) {
          this.newProperty();
        }
        // Cleanup the _ruleViewIsEditing flag
        this._ruleViewIsEditing = false;
      }, false);

      this.element.addEventListener("mousedown", () => {
        this.doc.defaultView.focus();
      }, false);

      // Create a property editor when the close brace is clicked.
      editableItem({ element: this.closeBrace }, () => {
        this.newProperty();
      });
    }
  },

  /**
   * Event handler called when a property changes on the
   * StyleRuleActor.
   */
  _locationChanged: function () {
    this.updateSourceLink();
  },

  updateSourceLink: function () {
    let sourceLabel = this.element.querySelector(".ruleview-rule-source-label");
    let title = this.rule.title;
    let sourceHref = (this.rule.sheet && this.rule.sheet.href) ?
      this.rule.sheet.href : title;
    let sourceLine = this.rule.ruleLine > 0 ? ":" + this.rule.ruleLine : "";

    sourceLabel.setAttribute("tooltiptext", sourceHref + sourceLine);

    if (this.toolbox.isToolRegistered("styleeditor")) {
      this.source.removeAttribute("unselectable");
    } else {
      this.source.setAttribute("unselectable", true);
    }

    if (this.rule.isSystem) {
      let uaLabel = _strings.GetStringFromName("rule.userAgentStyles");
      sourceLabel.setAttribute("value", uaLabel + " " + title);

      // Special case about:PreferenceStyleSheet, as it is generated on the
      // fly and the URI is not registered with the about: handler.
      // https://bugzilla.mozilla.org/show_bug.cgi?id=935803#c37
      if (sourceHref === "about:PreferenceStyleSheet") {
        this.source.setAttribute("unselectable", "true");
        sourceLabel.setAttribute("value", uaLabel);
        sourceLabel.removeAttribute("tooltiptext");
      }
    } else {
      sourceLabel.setAttribute("value", title);
      if (this.rule.ruleLine === -1 && this.rule.domRule.parentStyleSheet) {
        this.source.setAttribute("unselectable", "true");
      }
    }

    let showOrig = Services.prefs.getBoolPref(PREF_ORIG_SOURCES);
    if (showOrig && !this.rule.isSystem &&
        this.rule.domRule.type !== ELEMENT_STYLE) {
      // Only get the original source link if the right pref is set, if the rule
      // isn't a system rule and if it isn't an inline rule.
      this.rule.getOriginalSourceStrings().then((strings) => {
        sourceLabel.setAttribute("value", strings.short);
        sourceLabel.setAttribute("tooltiptext", strings.full);
      }, e => console.error(e)).then(() => {
        this.emit("source-link-updated");
      });
    } else {
      // If we're not getting the original source link, then we can emit the
      // event immediately (but still asynchronously to give consumers a chance
      // to register it after having instantiated the RuleEditor).
      promise.resolve().then(() => {
        this.emit("source-link-updated");
      });
    }
  },

  /**
   * Update the rule editor with the contents of the rule.
   */
  populate: function () {
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

        let containerClass =
          (this.rule.matchedSelectors.indexOf(selector) > -1) ?
          "ruleview-selector-matched" : "ruleview-selector-unmatched";
        let selectorContainer = createChild(this.selectorText, "span", {
          class: containerClass
        });

        let parsedSelector = parsePseudoClassesAndAttributes(selector);

        for (let selectorText of parsedSelector) {
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

    for (let prop of this.rule.textProps) {
      if (!prop.editor && !prop.invisible) {
        let editor = new TextPropertyEditor(this, prop);
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
  addProperty: function (name, value, priority, enabled, siblingProp) {
    let prop = this.rule.createProperty(name, value, priority, enabled,
      siblingProp);
    let index = this.rule.textProps.indexOf(prop);
    let editor = new TextPropertyEditor(this, prop);

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
  addProperties: function (properties, siblingProp) {
    if (!properties || !properties.length) {
      return;
    }

    let lastProp = siblingProp;
    for (let p of properties) {
      let isCommented = Boolean(p.commentOffsets);
      let enabled = !isCommented;
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
  newProperty: function () {
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
      popup: this.ruleView.popup
    });

    // Auto-close the input if multiple rules get pasted into new property.
    this.editor.input.addEventListener("paste",
      blurOnMultipleProperties(this.rule.cssProperties), false);
  },

  /**
   * Called when the new property input has been dismissed.
   *
   * @param {String} value
   *        The value in the editor.
   * @param {Boolean} commit
   *        True if the value should be committed.
   */
  _onNewProperty: function (value, commit) {
    if (!value || !commit) {
      return;
    }

    // parseDeclarations allows for name-less declarations, but in the present
    // case, we're creating a new declaration, it doesn't make sense to accept
    // these entries
    this.multipleAddedProperties =
      parseDeclarations(this.rule.cssProperties.isKnown, value, true)
      .filter(d => d.name);

    // Blur the editor field now and deal with adding declarations later when
    // the field gets destroyed (see _newPropertyDestroy)
    this.editor.input.blur();
  },

  /**
   * Called when the new property editor is destroyed.
   * This is where the properties (type TextProperty) are actually being
   * added, since we want to wait until after the inplace editor `destroy`
   * event has been fired to keep consistent UI state.
   */
  _newPropertyDestroy: function () {
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
  _onSelectorDone: function (value, commit, direction) {
    if (!commit || this.isEditing || value === "" ||
        value === this.rule.selectorText) {
      return;
    }

    let ruleView = this.ruleView;
    let elementStyle = ruleView._elementStyle;
    let element = elementStyle.element;
    let supportsUnmatchedRules =
      this.rule.domRule.supportsModifySelectorUnmatched;

    this.isEditing = true;

    this.rule.domRule.modifySelector(element, value).then(response => {
      this.isEditing = false;

      if (!supportsUnmatchedRules) {
        if (response) {
          this.ruleView.refreshPanel();
        }
        return;
      }

      let {ruleProps, isMatching} = response;
      if (!ruleProps) {
        // Notify for changes, even when nothing changes,
        // just to allow tests being able to track end of this request.
        ruleView.emit("ruleview-invalid-selector");
        return;
      }

      ruleProps.isUnmatched = !isMatching;
      let newRule = new Rule(elementStyle, ruleProps);
      let editor = new RuleEditor(ruleView, newRule);
      let rules = elementStyle.rules;

      rules.splice(rules.indexOf(this.rule), 1);
      rules.push(newRule);
      elementStyle._changed();
      elementStyle.markOverriddenAll();

      this.element.parentNode.replaceChild(editor.element, this.element);

      // Remove highlight for modified selector
      if (ruleView.highlightedSelector) {
        ruleView.toggleSelectorHighlighter(ruleView.lastSelectorIcon,
          ruleView.highlightedSelector);
      }

      editor._moveSelectorFocus(direction);
    }).then(null, err => {
      this.isEditing = false;
      promiseWarn(err);
    });
  },

  /**
   * Handle moving the focus change after a tab or return keypress in the
   * selector inplace editor.
   *
   * @param {Number} direction
   *        The move focus direction number.
   */
  _moveSelectorFocus: function (direction) {
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

exports.RuleEditor = RuleEditor;
