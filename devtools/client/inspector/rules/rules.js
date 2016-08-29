/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
/* globals gDevTools */

"use strict";

const promise = require("promise");
const defer = require("devtools/shared/defer");
const Services = require("Services");
const {Task} = require("devtools/shared/task");
const {Tools} = require("devtools/client/definitions");
const {l10n} = require("devtools/shared/inspector/css-logic");
const {ELEMENT_STYLE} = require("devtools/shared/specs/styles");
const {OutputParser} = require("devtools/client/shared/output-parser");
const {PrefObserver, PREF_ORIG_SOURCES} = require("devtools/client/styleeditor/utils");
const {ElementStyle} = require("devtools/client/inspector/rules/models/element-style");
const {Rule} = require("devtools/client/inspector/rules/models/rule");
const {RuleEditor} = require("devtools/client/inspector/rules/views/rule-editor");
const {createChild, promiseWarn, throttle} = require("devtools/client/inspector/shared/utils");
const {gDevTools} = require("devtools/client/framework/devtools");
const {getCssProperties} = require("devtools/shared/fronts/css-properties");

const overlays = require("devtools/client/inspector/shared/style-inspector-overlays");
const EventEmitter = require("devtools/shared/event-emitter");
const StyleInspectorMenu = require("devtools/client/inspector/shared/style-inspector-menu");
const {KeyShortcuts} = require("devtools/client/shared/key-shortcuts");
const clipboardHelper = require("devtools/shared/platform/clipboard");

const {AutocompletePopup} = require("devtools/client/shared/autocomplete-popup");

const HTML_NS = "http://www.w3.org/1999/xhtml";
const PREF_UA_STYLES = "devtools.inspector.showUserAgentStyles";
const PREF_DEFAULT_COLOR_UNIT = "devtools.defaultColorUnit";
const PREF_ENABLE_MDN_DOCS_TOOLTIP =
      "devtools.inspector.mdnDocsTooltip.enabled";
const FILTER_CHANGED_TIMEOUT = 150;

// This is used to parse user input when filtering.
const FILTER_PROP_RE = /\s*([^:\s]*)\s*:\s*(.*?)\s*;?$/;
// This is used to parse the filter search value to see if the filter
// should be strict or not
const FILTER_STRICT_RE = /\s*`(.*?)`\s*$/;

/**
 * Our model looks like this:
 *
 * ElementStyle:
 *   Responsible for keeping track of which properties are overridden.
 *   Maintains a list of Rule objects that apply to the element.
 * Rule:
 *   Manages a single style declaration or rule.
 *   Responsible for applying changes to the properties in a rule.
 *   Maintains a list of TextProperty objects.
 * TextProperty:
 *   Manages a single property from the authoredText attribute of the
 *     relevant declaration.
 *   Maintains a list of computed properties that come from this
 *     property declaration.
 *   Changes to the TextProperty are sent to its related Rule for
 *     application.
 *
 * View hierarchy mostly follows the model hierarchy.
 *
 * CssRuleView:
 *   Owns an ElementStyle and creates a list of RuleEditors for its
 *    Rules.
 * RuleEditor:
 *   Owns a Rule object and creates a list of TextPropertyEditors
 *     for its TextProperties.
 *   Manages creation of new text properties.
 * TextPropertyEditor:
 *   Owns a TextProperty object.
 *   Manages changes to the TextProperty.
 *   Can be expanded to display computed properties.
 *   Can mark a property disabled or enabled.
 */

/**
 * To figure out how shorthand properties are interpreted by the
 * engine, we will set properties on a dummy element and observe
 * how their .style attribute reflects them as computed values.
 * This function creates the document in which those dummy elements
 * will be created.
 */
var gDummyPromise;
function createDummyDocument() {
  if (gDummyPromise) {
    return gDummyPromise;
  }
  const { getDocShell, create: makeFrame } = require("sdk/frame/utils");

  let frame = makeFrame(Services.appShell.hiddenDOMWindow.document, {
    nodeName: "iframe",
    namespaceURI: "http://www.w3.org/1999/xhtml",
    allowJavascript: false,
    allowPlugins: false,
    allowAuth: false
  });
  let docShell = getDocShell(frame);
  let eventTarget = docShell.chromeEventHandler;
  let ssm = Services.scriptSecurityManager;

  // We probably need to call InheritFromDocShellToDoc to get the correct origin
  // attributes, but right now we can't call it from JS.
  let nullPrincipal = ssm.createNullPrincipal(docShell.getOriginAttributes());
  docShell.createAboutBlankContentViewer(nullPrincipal);
  let window = docShell.contentViewer.DOMDocument.defaultView;
  window.location = "data:text/html,<html></html>";
  let deferred = defer();
  eventTarget.addEventListener("DOMContentLoaded", function handler() {
    eventTarget.removeEventListener("DOMContentLoaded", handler, false);
    deferred.resolve(window.document);
    frame.remove();
  }, false);
  gDummyPromise = deferred.promise;
  return gDummyPromise;
}

/**
 * CssRuleView is a view of the style rules and declarations that
 * apply to a given element.  After construction, the 'element'
 * property will be available with the user interface.
 *
 * @param {Inspector} inspector
 *        Inspector toolbox panel
 * @param {Document} document
 *        The document that will contain the rule view.
 * @param {Object} store
 *        The CSS rule view can use this object to store metadata
 *        that might outlast the rule view, particularly the current
 *        set of disabled properties.
 * @param {PageStyleFront} pageStyle
 *        The PageStyleFront for communicating with the remote server.
 */
function CssRuleView(inspector, document, store, pageStyle) {
  this.inspector = inspector;
  this.styleDocument = document;
  this.styleWindow = this.styleDocument.defaultView;
  this.store = store || {};
  this.pageStyle = pageStyle;

  // Allow tests to override throttling behavior, as this can cause intermittents.
  this.throttle = throttle;

  this.cssProperties = getCssProperties(inspector.toolbox);

  this._outputParser = new OutputParser(document, this.cssProperties.supportsType);

  this._onAddRule = this._onAddRule.bind(this);
  this._onContextMenu = this._onContextMenu.bind(this);
  this._onCopy = this._onCopy.bind(this);
  this._onFilterStyles = this._onFilterStyles.bind(this);
  this._onClearSearch = this._onClearSearch.bind(this);
  this._onFilterTextboxContextMenu =
    this._onFilterTextboxContextMenu.bind(this);
  this._onTogglePseudoClassPanel = this._onTogglePseudoClassPanel.bind(this);
  this._onTogglePseudoClass = this._onTogglePseudoClass.bind(this);

  let doc = this.styleDocument;
  this.element = doc.getElementById("ruleview-container-focusable");
  this.addRuleButton = doc.getElementById("ruleview-add-rule-button");
  this.searchField = doc.getElementById("ruleview-searchbox");
  this.searchClearButton = doc.getElementById("ruleview-searchinput-clear");
  this.pseudoClassPanel = doc.getElementById("pseudo-class-panel");
  this.pseudoClassToggle = doc.getElementById("pseudo-class-panel-toggle");
  this.hoverCheckbox = doc.getElementById("pseudo-hover-toggle");
  this.activeCheckbox = doc.getElementById("pseudo-active-toggle");
  this.focusCheckbox = doc.getElementById("pseudo-focus-toggle");

  this.searchClearButton.hidden = true;

  this.shortcuts = new KeyShortcuts({ window: this.styleWindow });
  this._onShortcut = this._onShortcut.bind(this);
  this.shortcuts.on("Escape", this._onShortcut);
  this.shortcuts.on("Return", this._onShortcut);
  this.shortcuts.on("Space", this._onShortcut);
  this.shortcuts.on("CmdOrCtrl+F", this._onShortcut);
  this.element.addEventListener("copy", this._onCopy);
  this.element.addEventListener("contextmenu", this._onContextMenu);
  this.addRuleButton.addEventListener("click", this._onAddRule);
  this.searchField.addEventListener("input", this._onFilterStyles);
  this.searchField.addEventListener("contextmenu",
                                    this._onFilterTextboxContextMenu);
  this.searchClearButton.addEventListener("click", this._onClearSearch);
  this.pseudoClassToggle.addEventListener("click",
                                          this._onTogglePseudoClassPanel);
  this.hoverCheckbox.addEventListener("click", this._onTogglePseudoClass);
  this.activeCheckbox.addEventListener("click", this._onTogglePseudoClass);
  this.focusCheckbox.addEventListener("click", this._onTogglePseudoClass);

  this._handlePrefChange = this._handlePrefChange.bind(this);
  this._onSourcePrefChanged = this._onSourcePrefChanged.bind(this);

  this._prefObserver = new PrefObserver("devtools.");
  this._prefObserver.on(PREF_ORIG_SOURCES, this._onSourcePrefChanged);
  this._prefObserver.on(PREF_UA_STYLES, this._handlePrefChange);
  this._prefObserver.on(PREF_DEFAULT_COLOR_UNIT, this._handlePrefChange);
  this._prefObserver.on(PREF_ENABLE_MDN_DOCS_TOOLTIP, this._handlePrefChange);

  this.showUserAgentStyles = Services.prefs.getBoolPref(PREF_UA_STYLES);
  this.enableMdnDocsTooltip =
    Services.prefs.getBoolPref(PREF_ENABLE_MDN_DOCS_TOOLTIP);

  let options = {
    autoSelect: true,
    theme: "auto"
  };
  this.popup = new AutocompletePopup(inspector._toolbox, options);

  this._showEmpty();

  this._contextmenu = new StyleInspectorMenu(this, { isRuleView: true });

  // Add the tooltips and highlighters to the view
  this.tooltips = new overlays.TooltipsOverlay(this);
  this.tooltips.addToView();
  this.highlighters = new overlays.HighlightersOverlay(this);
  this.highlighters.addToView();

  EventEmitter.decorate(this);
}

CssRuleView.prototype = {
  // The element that we're inspecting.
  _viewedElement: null,

  // Used for cancelling timeouts in the style filter.
  _filterChangedTimeout: null,

  // Empty, unconnected element of the same type as this node, used
  // to figure out how shorthand properties will be parsed.
  _dummyElement: null,

  // Get the dummy elemenet.
  get dummyElement() {
    return this._dummyElement;
  },

  // Get the filter search value.
  get searchValue() {
    return this.searchField.value.toLowerCase();
  },

  /**
   * Get an instance of SelectorHighlighter (used to highlight nodes that match
   * selectors in the rule-view). A new instance is only created the first time
   * this function is called. The same instance will then be returned.
   *
   * @return {Promise} Resolves to the instance of the highlighter.
   */
  getSelectorHighlighter: Task.async(function* () {
    let utils = this.inspector.toolbox.highlighterUtils;
    if (!utils.supportsCustomHighlighters()) {
      return null;
    }

    if (this.selectorHighlighter) {
      return this.selectorHighlighter;
    }

    try {
      let h = yield utils.getHighlighterByType("SelectorHighlighter");
      this.selectorHighlighter = h;
      return h;
    } catch (e) {
      // The SelectorHighlighter type could not be created in the
      // current target.  It could be an older server, or a XUL page.
      return null;
    }
  }),

  /**
   * Highlight/unhighlight all the nodes that match a given set of selectors
   * inside the document of the current selected node.
   * Only one selector can be highlighted at a time, so calling the method a
   * second time with a different selector will first unhighlight the previously
   * highlighted nodes.
   * Calling the method a second time with the same selector will just
   * unhighlight the highlighted nodes.
   *
   * @param {DOMNode} selectorIcon
   *        The icon that was clicked to toggle the selector. The
   *        class 'highlighted' will be added when the selector is
   *        highlighted.
   * @param {String} selector
   *        The selector used to find nodes in the page.
   */
  toggleSelectorHighlighter: function (selectorIcon, selector) {
    if (this.lastSelectorIcon) {
      this.lastSelectorIcon.classList.remove("highlighted");
    }
    selectorIcon.classList.remove("highlighted");

    this.unhighlightSelector().then(() => {
      if (selector !== this.highlightedSelector) {
        this.highlightedSelector = selector;
        selectorIcon.classList.add("highlighted");
        this.lastSelectorIcon = selectorIcon;
        this.highlightSelector(selector).then(() => {
          this.emit("ruleview-selectorhighlighter-toggled", true);
        }, e => console.error(e));
      } else {
        this.highlightedSelector = null;
        this.emit("ruleview-selectorhighlighter-toggled", false);
      }
    }, e => console.error(e));
  },

  highlightSelector: Task.async(function* (selector) {
    let node = this.inspector.selection.nodeFront;

    let highlighter = yield this.getSelectorHighlighter();
    if (!highlighter) {
      return;
    }

    yield highlighter.show(node, {
      hideInfoBar: true,
      hideGuides: true,
      selector
    });
  }),

  unhighlightSelector: Task.async(function* () {
    let highlighter = yield this.getSelectorHighlighter();
    if (!highlighter) {
      return;
    }

    yield highlighter.hide();
  }),

  /**
   * Get the type of a given node in the rule-view
   *
   * @param {DOMNode} node
   *        The node which we want information about
   * @return {Object} The type information object contains the following props:
   * - type {String} One of the VIEW_NODE_XXX_TYPE const in
   *   style-inspector-overlays
   * - value {Object} Depends on the type of the node
   * returns null of the node isn't anything we care about
   */
  getNodeInfo: function (node) {
    if (!node) {
      return null;
    }

    let type, value;
    let classes = node.classList;
    let prop = getParentTextProperty(node);

    if (classes.contains("ruleview-propertyname") && prop) {
      type = overlays.VIEW_NODE_PROPERTY_TYPE;
      value = {
        property: node.textContent,
        value: getPropertyNameAndValue(node).value,
        enabled: prop.enabled,
        overridden: prop.overridden,
        pseudoElement: prop.rule.pseudoElement,
        sheetHref: prop.rule.domRule.href,
        textProperty: prop
      };
    } else if (classes.contains("ruleview-propertyvalue") && prop) {
      type = overlays.VIEW_NODE_VALUE_TYPE;
      value = {
        property: getPropertyNameAndValue(node).name,
        value: node.textContent,
        enabled: prop.enabled,
        overridden: prop.overridden,
        pseudoElement: prop.rule.pseudoElement,
        sheetHref: prop.rule.domRule.href,
        textProperty: prop
      };
    } else if (classes.contains("theme-link") &&
               !classes.contains("ruleview-rule-source") && prop) {
      type = overlays.VIEW_NODE_IMAGE_URL_TYPE;
      value = {
        property: getPropertyNameAndValue(node).name,
        value: node.parentNode.textContent,
        url: node.href,
        enabled: prop.enabled,
        overridden: prop.overridden,
        pseudoElement: prop.rule.pseudoElement,
        sheetHref: prop.rule.domRule.href,
        textProperty: prop
      };
    } else if (classes.contains("ruleview-selector-unmatched") ||
               classes.contains("ruleview-selector-matched") ||
               classes.contains("ruleview-selectorcontainer") ||
               classes.contains("ruleview-selector") ||
               classes.contains("ruleview-selector-attribute") ||
               classes.contains("ruleview-selector-pseudo-class") ||
               classes.contains("ruleview-selector-pseudo-class-lock")) {
      type = overlays.VIEW_NODE_SELECTOR_TYPE;
      value = this._getRuleEditorForNode(node).selectorText.textContent;
    } else if (classes.contains("ruleview-rule-source") ||
               classes.contains("ruleview-rule-source-label")) {
      type = overlays.VIEW_NODE_LOCATION_TYPE;
      let rule = this._getRuleEditorForNode(node).rule;
      value = (rule.sheet && rule.sheet.href) ? rule.sheet.href : rule.title;
    } else {
      return null;
    }

    return {type, value};
  },

  /**
   * Retrieve the RuleEditor instance that should be stored on
   * the offset parent of the node
   */
  _getRuleEditorForNode: function (node) {
    if (!node.offsetParent) {
      // some nodes don't have an offsetParent, but their parentNode does
      node = node.parentNode;
    }
    return node.offsetParent._ruleEditor;
  },

  /**
   * Context menu handler.
   */
  _onContextMenu: function (event) {
    this._contextmenu.show(event);
  },

  /**
   * Callback for copy event. Copy the selected text.
   *
   * @param {Event} event
   *        copy event object.
   */
  _onCopy: function (event) {
    if (event) {
      this.copySelection(event.target);
      event.preventDefault();
    }
  },

  /**
   * Copy the current selection. The current target is necessary
   * if the selection is inside an input or a textarea
   *
   * @param {DOMNode} target
   *        DOMNode target of the copy action
   */
  copySelection: function (target) {
    try {
      let text = "";

      let nodeName = target && target.nodeName;
      if (nodeName === "input" || nodeName == "textarea") {
        let start = Math.min(target.selectionStart, target.selectionEnd);
        let end = Math.max(target.selectionStart, target.selectionEnd);
        let count = end - start;
        text = target.value.substr(start, count);
      } else {
        text = this.styleWindow.getSelection().toString();

        // Remove any double newlines.
        text = text.replace(/(\r?\n)\r?\n/g, "$1");
      }

      clipboardHelper.copyString(text);
    } catch (e) {
      console.error(e);
    }
  },

  /**
   * A helper for _onAddRule that handles the case where the actor
   * does not support as-authored styles.
   */
  _onAddNewRuleNonAuthored: function () {
    let elementStyle = this._elementStyle;
    let element = elementStyle.element;
    let rules = elementStyle.rules;
    let pseudoClasses = element.pseudoClassLocks;

    this.pageStyle.addNewRule(element, pseudoClasses).then(options => {
      let newRule = new Rule(elementStyle, options);
      rules.push(newRule);
      let editor = new RuleEditor(this, newRule);
      newRule.editor = editor;

      // Insert the new rule editor after the inline element rule
      if (rules.length <= 1) {
        this.element.appendChild(editor.element);
      } else {
        for (let rule of rules) {
          if (rule.domRule.type === ELEMENT_STYLE) {
            let referenceElement = rule.editor.element.nextSibling;
            this.element.insertBefore(editor.element, referenceElement);
            break;
          }
        }
      }

      // Focus and make the new rule's selector editable
      editor.selectorText.click();
      elementStyle._changed();
    });
  },

  /**
   * Add a new rule to the current element.
   */
  _onAddRule: function () {
    let elementStyle = this._elementStyle;
    let element = elementStyle.element;
    let client = this.inspector.toolbox._target.client;
    let pseudoClasses = element.pseudoClassLocks;

    if (!client.traits.addNewRule) {
      return;
    }

    if (!this.pageStyle.supportsAuthoredStyles) {
      // We're talking to an old server.
      this._onAddNewRuleNonAuthored();
      return;
    }

    // Adding a new rule with authored styles will cause the actor to
    // emit an event, which will in turn cause the rule view to be
    // updated.  So, we wait for this update and for the rule creation
    // request to complete, and then focus the new rule's selector.
    let eventPromise = this.once("ruleview-refreshed");
    let newRulePromise = this.pageStyle.addNewRule(element, pseudoClasses);
    promise.all([eventPromise, newRulePromise]).then((values) => {
      let options = values[1];
      // Be sure the reference the correct |rules| here.
      for (let rule of this._elementStyle.rules) {
        if (options.rule === rule.domRule) {
          rule.editor.selectorText.click();
          elementStyle._changed();
          break;
        }
      }
    });
  },

  /**
   * Disables add rule button when needed
   */
  refreshAddRuleButtonState: function () {
    let shouldBeDisabled = !this._viewedElement ||
                           !this.inspector.selection.isElementNode() ||
                           this.inspector.selection.isAnonymousNode();
    this.addRuleButton.disabled = shouldBeDisabled;
  },

  setPageStyle: function (pageStyle) {
    this.pageStyle = pageStyle;
  },

  /**
   * Return {Boolean} true if the rule view currently has an input
   * editor visible.
   */
  get isEditing() {
    return this.tooltips.isEditing ||
      this.element.querySelectorAll(".styleinspector-propertyeditor")
        .length > 0;
  },

  _handlePrefChange: function (pref) {
    if (pref === PREF_UA_STYLES) {
      this.showUserAgentStyles = Services.prefs.getBoolPref(pref);
    }

    // Reselect the currently selected element
    let refreshOnPrefs = [PREF_UA_STYLES, PREF_DEFAULT_COLOR_UNIT];
    if (refreshOnPrefs.indexOf(pref) > -1) {
      this.selectElement(this._viewedElement, true);
    }
  },

  /**
   * Update source links when pref for showing original sources changes
   */
  _onSourcePrefChanged: function () {
    if (this._elementStyle && this._elementStyle.rules) {
      for (let rule of this._elementStyle.rules) {
        if (rule.editor) {
          rule.editor.updateSourceLink();
        }
      }
      this.inspector.emit("rule-view-sourcelinks-updated");
    }
  },

  /**
   * Set the filter style search value.
   * @param {String} value
   *        The search value.
   */
  setFilterStyles: function (value = "") {
    this.searchField.value = value;
    this.searchField.focus();
    this._onFilterStyles();
  },

  /**
   * Called when the user enters a search term in the filter style search box.
   */
  _onFilterStyles: function () {
    if (this._filterChangedTimeout) {
      clearTimeout(this._filterChangedTimeout);
    }

    let filterTimeout = (this.searchValue.length > 0) ?
                        FILTER_CHANGED_TIMEOUT : 0;
    this.searchClearButton.hidden = this.searchValue.length === 0;

    this._filterChangedTimeout = setTimeout(() => {
      if (this.searchField.value.length > 0) {
        this.searchField.setAttribute("filled", true);
      } else {
        this.searchField.removeAttribute("filled");
      }

      this.searchData = {
        searchPropertyMatch: FILTER_PROP_RE.exec(this.searchValue),
        searchPropertyName: this.searchValue,
        searchPropertyValue: this.searchValue,
        strictSearchValue: "",
        strictSearchPropertyName: false,
        strictSearchPropertyValue: false,
        strictSearchAllValues: false
      };

      if (this.searchData.searchPropertyMatch) {
        // Parse search value as a single property line and extract the
        // property name and value. If the parsed property name or value is
        // contained in backquotes (`), extract the value within the backquotes
        // and set the corresponding strict search for the property to true.
        if (FILTER_STRICT_RE.test(this.searchData.searchPropertyMatch[1])) {
          this.searchData.strictSearchPropertyName = true;
          this.searchData.searchPropertyName =
            FILTER_STRICT_RE.exec(this.searchData.searchPropertyMatch[1])[1];
        } else {
          this.searchData.searchPropertyName =
            this.searchData.searchPropertyMatch[1];
        }

        if (FILTER_STRICT_RE.test(this.searchData.searchPropertyMatch[2])) {
          this.searchData.strictSearchPropertyValue = true;
          this.searchData.searchPropertyValue =
            FILTER_STRICT_RE.exec(this.searchData.searchPropertyMatch[2])[1];
        } else {
          this.searchData.searchPropertyValue =
            this.searchData.searchPropertyMatch[2];
        }

        // Strict search for stylesheets will match the property line regex.
        // Extract the search value within the backquotes to be used
        // in the strict search for stylesheets in _highlightStyleSheet.
        if (FILTER_STRICT_RE.test(this.searchValue)) {
          this.searchData.strictSearchValue =
            FILTER_STRICT_RE.exec(this.searchValue)[1];
        }
      } else if (FILTER_STRICT_RE.test(this.searchValue)) {
        // If the search value does not correspond to a property line and
        // is contained in backquotes, extract the search value within the
        // backquotes and set the flag to perform a strict search for all
        // the values (selector, stylesheet, property and computed values).
        let searchValue = FILTER_STRICT_RE.exec(this.searchValue)[1];
        this.searchData.strictSearchAllValues = true;
        this.searchData.searchPropertyName = searchValue;
        this.searchData.searchPropertyValue = searchValue;
        this.searchData.strictSearchValue = searchValue;
      }

      this._clearHighlight(this.element);
      this._clearRules();
      this._createEditors();

      this.inspector.emit("ruleview-filtered");

      this._filterChangeTimeout = null;
    }, filterTimeout);
  },

  /**
   * Context menu handler for filter style search box.
   */
  _onFilterTextboxContextMenu: function (event) {
    try {
      this.styleWindow.focus();
      let contextmenu = this.inspector.toolbox.textboxContextMenuPopup;
      contextmenu.openPopupAtScreen(event.screenX, event.screenY, true);
    } catch (e) {
      console.error(e);
    }
  },

  /**
   * Called when the user clicks on the clear button in the filter style search
   * box. Returns true if the search box is cleared and false otherwise.
   */
  _onClearSearch: function () {
    if (this.searchField.value) {
      this.setFilterStyles("");
      return true;
    }

    return false;
  },

  destroy: function () {
    this.isDestroyed = true;
    this.clear();

    this._dummyElement = null;
    this.dummyElementPromise = null;
    gDummyPromise = null;

    this._prefObserver.off(PREF_ORIG_SOURCES, this._onSourcePrefChanged);
    this._prefObserver.off(PREF_UA_STYLES, this._handlePrefChange);
    this._prefObserver.off(PREF_DEFAULT_COLOR_UNIT, this._handlePrefChange);
    this._prefObserver.destroy();

    this._outputParser = null;

    // Remove context menu
    if (this._contextmenu) {
      this._contextmenu.destroy();
      this._contextmenu = null;
    }

    this.tooltips.destroy();
    this.highlighters.destroy();

    // Remove bound listeners
    this.shortcuts.destroy();
    this.element.removeEventListener("copy", this._onCopy);
    this.element.removeEventListener("contextmenu", this._onContextMenu);
    this.addRuleButton.removeEventListener("click", this._onAddRule);
    this.searchField.removeEventListener("input", this._onFilterStyles);
    this.searchField.removeEventListener("contextmenu",
      this._onFilterTextboxContextMenu);
    this.searchClearButton.removeEventListener("click", this._onClearSearch);
    this.pseudoClassToggle.removeEventListener("click",
      this._onTogglePseudoClassPanel);
    this.hoverCheckbox.removeEventListener("click", this._onTogglePseudoClass);
    this.activeCheckbox.removeEventListener("click", this._onTogglePseudoClass);
    this.focusCheckbox.removeEventListener("click", this._onTogglePseudoClass);

    this.searchField = null;
    this.searchClearButton = null;
    this.pseudoClassPanel = null;
    this.pseudoClassToggle = null;
    this.hoverCheckbox = null;
    this.activeCheckbox = null;
    this.focusCheckbox = null;

    this.inspector = null;
    this.styleDocument = null;
    this.styleWindow = null;

    if (this.element.parentNode) {
      this.element.parentNode.removeChild(this.element);
    }

    if (this._elementStyle) {
      this._elementStyle.destroy();
    }

    this.popup.destroy();
  },

  /**
   * Mark the view as selecting an element, disabling all interaction, and
   * visually clearing the view after a few milliseconds to avoid confusion
   * about which element's styles the rule view shows.
   */
  _startSelectingElement: function () {
    this.element.classList.add("non-interactive");
  },

  /**
   * Mark the view as no longer selecting an element, re-enabling interaction.
   */
  _stopSelectingElement: function () {
    this.element.classList.remove("non-interactive");
  },

  /**
   * Update the view with a new selected element.
   *
   * @param {NodeActor} element
   *        The node whose style rules we'll inspect.
   * @param {Boolean} allowRefresh
   *        Update the view even if the element is the same as last time.
   */
  selectElement: function (element, allowRefresh = false) {
    let refresh = (this._viewedElement === element);
    if (refresh && !allowRefresh) {
      return promise.resolve(undefined);
    }

    if (this.popup.isOpen) {
      this.popup.hidePopup();
    }

    this.clear(false);
    this._viewedElement = element;

    this.clearPseudoClassPanel();
    this.refreshAddRuleButtonState();

    if (!this._viewedElement) {
      this._stopSelectingElement();
      this._clearRules();
      this._showEmpty();
      this.refreshPseudoClassPanel();
      return promise.resolve(undefined);
    }

    // To figure out how shorthand properties are interpreted by the
    // engine, we will set properties on a dummy element and observe
    // how their .style attribute reflects them as computed values.
    this.dummyElementPromise = createDummyDocument().then(document => {
      // ::before and ::after do not have a namespaceURI
      let namespaceURI = this.element.namespaceURI ||
          document.documentElement.namespaceURI;
      this._dummyElement = document.createElementNS(namespaceURI,
                                                   this.element.tagName);
      document.documentElement.appendChild(this._dummyElement);
      return this._dummyElement;
    }).then(null, promiseWarn);

    let elementStyle = new ElementStyle(element, this, this.store,
      this.pageStyle, this.showUserAgentStyles);
    this._elementStyle = elementStyle;

    this._startSelectingElement();

    return this.dummyElementPromise.then(() => {
      if (this._elementStyle === elementStyle) {
        return this._populate();
      }
      return undefined;
    }).then(() => {
      if (this._elementStyle === elementStyle) {
        if (!refresh) {
          this.element.scrollTop = 0;
        }
        this._stopSelectingElement();
        this._elementStyle.onChanged = () => {
          this._changed();
        };
      }
    }).then(null, e => {
      if (this._elementStyle === elementStyle) {
        this._stopSelectingElement();
        this._clearRules();
      }
      console.error(e);
    });
  },

  /**
   * Update the rules for the currently highlighted element.
   */
  refreshPanel: function () {
    // Ignore refreshes during editing or when no element is selected.
    if (this.isEditing || !this._elementStyle) {
      return promise.resolve(undefined);
    }

    // Repopulate the element style once the current modifications are done.
    let promises = [];
    for (let rule of this._elementStyle.rules) {
      if (rule._applyingModifications) {
        promises.push(rule._applyingModifications);
      }
    }

    return promise.all(promises).then(() => {
      return this._populate();
    });
  },

  /**
   * Clear the pseudo class options panel by removing the checked and disabled
   * attributes for each checkbox.
   */
  clearPseudoClassPanel: function () {
    this.hoverCheckbox.checked = this.hoverCheckbox.disabled = false;
    this.activeCheckbox.checked = this.activeCheckbox.disabled = false;
    this.focusCheckbox.checked = this.focusCheckbox.disabled = false;
  },

  /**
   * Update the pseudo class options for the currently highlighted element.
   */
  refreshPseudoClassPanel: function () {
    if (!this._elementStyle || !this.inspector.selection.isElementNode()) {
      this.hoverCheckbox.disabled = true;
      this.activeCheckbox.disabled = true;
      this.focusCheckbox.disabled = true;
      return;
    }

    for (let pseudoClassLock of this._elementStyle.element.pseudoClassLocks) {
      switch (pseudoClassLock) {
        case ":hover": {
          this.hoverCheckbox.checked = true;
          break;
        }
        case ":active": {
          this.activeCheckbox.checked = true;
          break;
        }
        case ":focus": {
          this.focusCheckbox.checked = true;
          break;
        }
      }
    }
  },

  _populate: function () {
    let elementStyle = this._elementStyle;
    return this._elementStyle.populate().then(() => {
      if (this._elementStyle !== elementStyle || this.isDestroyed) {
        return null;
      }

      this._clearRules();
      let onEditorsReady = this._createEditors();
      this.refreshPseudoClassPanel();

      // Notify anyone that cares that we refreshed.
      return onEditorsReady.then(() => {
        this.emit("ruleview-refreshed");
      }, e => console.error(e));
    }).then(null, promiseWarn);
  },

  /**
   * Show the user that the rule view has no node selected.
   */
  _showEmpty: function () {
    if (this.styleDocument.getElementById("ruleview-no-results")) {
      return;
    }

    createChild(this.element, "div", {
      id: "ruleview-no-results",
      textContent: l10n("rule.empty")
    });
  },

  /**
   * Clear the rules.
   */
  _clearRules: function () {
    this.element.innerHTML = "";
  },

  /**
   * Clear the rule view.
   */
  clear: function (clearDom = true) {
    this.lastSelectorIcon = null;

    if (clearDom) {
      this._clearRules();
    }
    this._viewedElement = null;

    if (this._elementStyle) {
      this._elementStyle.destroy();
      this._elementStyle = null;
    }
  },

  /**
   * Called when the user has made changes to the ElementStyle.
   * Emits an event that clients can listen to.
   */
  _changed: function () {
    this.emit("ruleview-changed");
  },

  /**
   * Text for header that shows above rules for this element
   */
  get selectedElementLabel() {
    if (this._selectedElementLabel) {
      return this._selectedElementLabel;
    }
    this._selectedElementLabel = l10n("rule.selectedElement");
    return this._selectedElementLabel;
  },

  /**
   * Text for header that shows above rules for pseudo elements
   */
  get pseudoElementLabel() {
    if (this._pseudoElementLabel) {
      return this._pseudoElementLabel;
    }
    this._pseudoElementLabel = l10n("rule.pseudoElement");
    return this._pseudoElementLabel;
  },

  get showPseudoElements() {
    if (this._showPseudoElements === undefined) {
      this._showPseudoElements =
        Services.prefs.getBoolPref("devtools.inspector.show_pseudo_elements");
    }
    return this._showPseudoElements;
  },

  /**
   * Creates an expandable container in the rule view
   *
   * @param  {String} label
   *         The label for the container header
   * @param  {Boolean} isPseudo
   *         Whether or not the container will hold pseudo element rules
   * @return {DOMNode} The container element
   */
  createExpandableContainer: function (label, isPseudo = false) {
    let header = this.styleDocument.createElementNS(HTML_NS, "div");
    header.className = this._getRuleViewHeaderClassName(true);
    header.textContent = label;

    let twisty = this.styleDocument.createElementNS(HTML_NS, "span");
    twisty.className = "ruleview-expander theme-twisty";
    twisty.setAttribute("open", "true");

    header.insertBefore(twisty, header.firstChild);
    this.element.appendChild(header);

    let container = this.styleDocument.createElementNS(HTML_NS, "div");
    container.classList.add("ruleview-expandable-container");
    container.hidden = false;
    this.element.appendChild(container);

    header.addEventListener("dblclick", () => {
      this._toggleContainerVisibility(twisty, container, isPseudo,
        !this.showPseudoElements);
    }, false);

    twisty.addEventListener("click", () => {
      this._toggleContainerVisibility(twisty, container, isPseudo,
        !this.showPseudoElements);
    }, false);

    if (isPseudo) {
      this._toggleContainerVisibility(twisty, container, isPseudo,
        this.showPseudoElements);
    }

    return container;
  },

  /**
   * Toggle the visibility of an expandable container
   *
   * @param  {DOMNode}  twisty
   *         Clickable toggle DOM Node
   * @param  {DOMNode}  container
   *         Expandable container DOM Node
   * @param  {Boolean}  isPseudo
   *         Whether or not the container will hold pseudo element rules
   * @param  {Boolean}  showPseudo
   *         Whether or not pseudo element rules should be displayed
   */
  _toggleContainerVisibility: function (twisty, container, isPseudo,
      showPseudo) {
    let isOpen = twisty.getAttribute("open");

    if (isPseudo) {
      this._showPseudoElements = !!showPseudo;

      Services.prefs.setBoolPref("devtools.inspector.show_pseudo_elements",
        this.showPseudoElements);

      container.hidden = !this.showPseudoElements;
      isOpen = !this.showPseudoElements;
    } else {
      container.hidden = !container.hidden;
    }

    if (isOpen) {
      twisty.removeAttribute("open");
    } else {
      twisty.setAttribute("open", "true");
    }
  },

  _getRuleViewHeaderClassName: function (isPseudo) {
    let baseClassName = "theme-gutter ruleview-header";
    return isPseudo ? baseClassName + " ruleview-expandable-header" :
      baseClassName;
  },

  /**
   * Creates editor UI for each of the rules in _elementStyle.
   */
  _createEditors: function () {
    // Run through the current list of rules, attaching
    // their editors in order.  Create editors if needed.
    let lastInheritedSource = "";
    let lastKeyframes = null;
    let seenPseudoElement = false;
    let seenNormalElement = false;
    let seenSearchTerm = false;
    let container = null;

    if (!this._elementStyle.rules) {
      return promise.resolve();
    }

    let editorReadyPromises = [];
    for (let rule of this._elementStyle.rules) {
      if (rule.domRule.system) {
        continue;
      }

      // Initialize rule editor
      if (!rule.editor) {
        rule.editor = new RuleEditor(this, rule);
        editorReadyPromises.push(rule.editor.once("source-link-updated"));
      }

      // Filter the rules and highlight any matches if there is a search input
      if (this.searchValue && this.searchData) {
        if (this.highlightRule(rule)) {
          seenSearchTerm = true;
        } else if (rule.domRule.type !== ELEMENT_STYLE) {
          continue;
        }
      }

      // Only print header for this element if there are pseudo elements
      if (seenPseudoElement && !seenNormalElement && !rule.pseudoElement) {
        seenNormalElement = true;
        let div = this.styleDocument.createElementNS(HTML_NS, "div");
        div.className = this._getRuleViewHeaderClassName();
        div.textContent = this.selectedElementLabel;
        this.element.appendChild(div);
      }

      let inheritedSource = rule.inheritedSource;
      if (inheritedSource && inheritedSource !== lastInheritedSource) {
        let div = this.styleDocument.createElementNS(HTML_NS, "div");
        div.className = this._getRuleViewHeaderClassName();
        div.textContent = inheritedSource;
        lastInheritedSource = inheritedSource;
        this.element.appendChild(div);
      }

      if (!seenPseudoElement && rule.pseudoElement) {
        seenPseudoElement = true;
        container = this.createExpandableContainer(this.pseudoElementLabel,
                                                   true);
      }

      let keyframes = rule.keyframes;
      if (keyframes && keyframes !== lastKeyframes) {
        lastKeyframes = keyframes;
        container = this.createExpandableContainer(rule.keyframesName);
      }

      if (container && (rule.pseudoElement || keyframes)) {
        container.appendChild(rule.editor.element);
      } else {
        this.element.appendChild(rule.editor.element);
      }
    }

    if (this.searchValue && !seenSearchTerm) {
      this.searchField.classList.add("devtools-style-searchbox-no-match");
    } else {
      this.searchField.classList.remove("devtools-style-searchbox-no-match");
    }

    return promise.all(editorReadyPromises);
  },

  /**
   * Highlight rules that matches the filter search value and returns a
   * boolean indicating whether or not rules were highlighted.
   *
   * @param  {Rule} rule
   *         The rule object we're highlighting if its rule selectors or
   *         property values match the search value.
   * @return {Boolean} true if the rule was highlighted, false otherwise.
   */
  highlightRule: function (rule) {
    let isRuleSelectorHighlighted = this._highlightRuleSelector(rule);
    let isStyleSheetHighlighted = this._highlightStyleSheet(rule);
    let isHighlighted = isRuleSelectorHighlighted || isStyleSheetHighlighted;

    // Highlight search matches in the rule properties
    for (let textProp of rule.textProps) {
      if (!textProp.invisible && this._highlightProperty(textProp.editor)) {
        isHighlighted = true;
      }
    }

    return isHighlighted;
  },

  /**
   * Highlights the rule selector that matches the filter search value and
   * returns a boolean indicating whether or not the selector was highlighted.
   *
   * @param  {Rule} rule
   *         The Rule object.
   * @return {Boolean} true if the rule selector was highlighted,
   *         false otherwise.
   */
  _highlightRuleSelector: function (rule) {
    let isSelectorHighlighted = false;

    let selectorNodes = [...rule.editor.selectorText.childNodes];
    if (rule.domRule.type === CSSRule.KEYFRAME_RULE) {
      selectorNodes = [rule.editor.selectorText];
    } else if (rule.domRule.type === ELEMENT_STYLE) {
      selectorNodes = [];
    }

    // Highlight search matches in the rule selectors
    for (let selectorNode of selectorNodes) {
      let selector = selectorNode.textContent.toLowerCase();
      if ((this.searchData.strictSearchAllValues &&
           selector === this.searchData.strictSearchValue) ||
          (!this.searchData.strictSearchAllValues &&
           selector.includes(this.searchValue))) {
        selectorNode.classList.add("ruleview-highlight");
        isSelectorHighlighted = true;
      }
    }

    return isSelectorHighlighted;
  },

  /**
   * Highlights the stylesheet source that matches the filter search value and
   * returns a boolean indicating whether or not the stylesheet source was
   * highlighted.
   *
   * @return {Boolean} true if the stylesheet source was highlighted, false
   *         otherwise.
   */
  _highlightStyleSheet: function (rule) {
    let styleSheetSource = rule.title.toLowerCase();
    let isStyleSheetHighlighted = this.searchData.strictSearchValue ?
      styleSheetSource === this.searchData.strictSearchValue :
      styleSheetSource.includes(this.searchValue);

    if (isStyleSheetHighlighted) {
      rule.editor.source.classList.add("ruleview-highlight");
    }

    return isStyleSheetHighlighted;
  },

  /**
   * Highlights the rule properties and computed properties that match the
   * filter search value and returns a boolean indicating whether or not the
   * property or computed property was highlighted.
   *
   * @param  {TextPropertyEditor} editor
   *         The rule property TextPropertyEditor object.
   * @return {Boolean} true if the property or computed property was
   *         highlighted, false otherwise.
   */
  _highlightProperty: function (editor) {
    let isPropertyHighlighted = this._highlightRuleProperty(editor);
    let isComputedHighlighted = this._highlightComputedProperty(editor);

    // Expand the computed list if a computed property is highlighted and the
    // property rule is not highlighted
    if (!isPropertyHighlighted && isComputedHighlighted &&
        !editor.computed.hasAttribute("user-open")) {
      editor.expandForFilter();
    }

    return isPropertyHighlighted || isComputedHighlighted;
  },

  /**
   * Called when TextPropertyEditor is updated and updates the rule property
   * highlight.
   *
   * @param  {TextPropertyEditor} editor
   *         The rule property TextPropertyEditor object.
   */
  _updatePropertyHighlight: function (editor) {
    if (!this.searchValue || !this.searchData) {
      return;
    }

    this._clearHighlight(editor.element);

    if (this._highlightProperty(editor)) {
      this.searchField.classList.remove("devtools-style-searchbox-no-match");
    }
  },

  /**
   * Highlights the rule property that matches the filter search value
   * and returns a boolean indicating whether or not the property was
   * highlighted.
   *
   * @param  {TextPropertyEditor} editor
   *         The rule property TextPropertyEditor object.
   * @return {Boolean} true if the rule property was highlighted,
   *         false otherwise.
   */
  _highlightRuleProperty: function (editor) {
    // Get the actual property value displayed in the rule view
    let propertyName = editor.prop.name.toLowerCase();
    let propertyValue = editor.valueSpan.textContent.toLowerCase();

    return this._highlightMatches(editor.container, propertyName,
                                  propertyValue);
  },

  /**
   * Highlights the computed property that matches the filter search value and
   * returns a boolean indicating whether or not the computed property was
   * highlighted.
   *
   * @param  {TextPropertyEditor} editor
   *         The rule property TextPropertyEditor object.
   * @return {Boolean} true if the computed property was highlighted, false
   *         otherwise.
   */
  _highlightComputedProperty: function (editor) {
    let isComputedHighlighted = false;

    // Highlight search matches in the computed list of properties
    editor._populateComputed();
    for (let computed of editor.prop.computed) {
      if (computed.element) {
        // Get the actual property value displayed in the computed list
        let computedName = computed.name.toLowerCase();
        let computedValue = computed.parsedValue.toLowerCase();

        isComputedHighlighted = this._highlightMatches(computed.element,
          computedName, computedValue) ? true : isComputedHighlighted;
      }
    }

    return isComputedHighlighted;
  },

  /**
   * Helper function for highlightRules that carries out highlighting the given
   * element if the search terms match the property, and returns a boolean
   * indicating whether or not the search terms match.
   *
   * @param  {DOMNode} element
   *         The node to highlight if search terms match
   * @param  {String} propertyName
   *         The property name of a rule
   * @param  {String} propertyValue
   *         The property value of a rule
   * @return {Boolean} true if the given search terms match the property, false
   *         otherwise.
   */
  _highlightMatches: function (element, propertyName, propertyValue) {
    let {
      searchPropertyName,
      searchPropertyValue,
      searchPropertyMatch,
      strictSearchPropertyName,
      strictSearchPropertyValue,
      strictSearchAllValues,
    } = this.searchData;
    let matches = false;

    // If the inputted search value matches a property line like
    // `font-family: arial`, then check to make sure the name and value match.
    // Otherwise, just compare the inputted search string directly against the
    // name and value of the rule property.
    let hasNameAndValue = searchPropertyMatch &&
                          searchPropertyName &&
                          searchPropertyValue;
    let isMatch = (value, query, isStrict) => {
      return isStrict ? value === query : query && value.includes(query);
    };

    if (hasNameAndValue) {
      matches =
        isMatch(propertyName, searchPropertyName, strictSearchPropertyName) &&
        isMatch(propertyValue, searchPropertyValue, strictSearchPropertyValue);
    } else {
      matches =
        isMatch(propertyName, searchPropertyName,
                strictSearchPropertyName || strictSearchAllValues) ||
        isMatch(propertyValue, searchPropertyValue,
                strictSearchPropertyValue || strictSearchAllValues);
    }

    if (matches) {
      element.classList.add("ruleview-highlight");
    }

    return matches;
  },

  /**
   * Clear all search filter highlights in the panel, and close the computed
   * list if toggled opened
   */
  _clearHighlight: function (element) {
    for (let el of element.querySelectorAll(".ruleview-highlight")) {
      el.classList.remove("ruleview-highlight");
    }

    for (let computed of element.querySelectorAll(
          ".ruleview-computedlist[filter-open]")) {
      computed.parentNode._textPropertyEditor.collapseForFilter();
    }
  },

  /**
   * Called when the pseudo class panel button is clicked and toggles
   * the display of the pseudo class panel.
   */
  _onTogglePseudoClassPanel: function () {
    if (this.pseudoClassPanel.hidden) {
      this.pseudoClassToggle.setAttribute("checked", "true");
      this.hoverCheckbox.setAttribute("tabindex", "0");
      this.activeCheckbox.setAttribute("tabindex", "0");
      this.focusCheckbox.setAttribute("tabindex", "0");
    } else {
      this.pseudoClassToggle.removeAttribute("checked");
      this.hoverCheckbox.setAttribute("tabindex", "-1");
      this.activeCheckbox.setAttribute("tabindex", "-1");
      this.focusCheckbox.setAttribute("tabindex", "-1");
    }

    this.pseudoClassPanel.hidden = !this.pseudoClassPanel.hidden;
  },

  /**
   * Called when a pseudo class checkbox is clicked and toggles
   * the pseudo class for the current selected element.
   */
  _onTogglePseudoClass: function (event) {
    let target = event.currentTarget;
    this.inspector.togglePseudoClass(target.value);
  },

  /**
   * Handle the keypress event in the rule view.
   */
  _onShortcut: function (name, event) {
    if (!event.target.closest("#sidebar-panel-ruleview")) {
      return;
    }

    if (name === "CmdOrCtrl+F") {
      this.searchField.focus();
      event.preventDefault();
    } else if ((name === "Return" || name === "Space") &&
               this.element.classList.contains("non-interactive")) {
      event.preventDefault();
    } else if (name === "Escape" &&
               event.target === this.searchField &&
               this._onClearSearch()) {
      // Handle the search box's keypress event. If the escape key is pressed,
      // clear the search box field.
      event.preventDefault();
      event.stopPropagation();
    }
  }

};

/**
 * Helper functions
 */

/**
 * Walk up the DOM from a given node until a parent property holder is found.
 * For elements inside the computed property list, the non-computed parent
 * property holder will be returned
 *
 * @param {DOMNode} node
 *        The node to start from
 * @return {DOMNode} The parent property holder node, or null if not found
 */
function getParentTextPropertyHolder(node) {
  while (true) {
    if (!node || !node.classList) {
      return null;
    }
    if (node.classList.contains("ruleview-property")) {
      return node;
    }
    node = node.parentNode;
  }
}

/**
 * For any given node, find the TextProperty it is in if any
 * @param {DOMNode} node
 *        The node to start from
 * @return {TextProperty}
 */
function getParentTextProperty(node) {
  let parent = getParentTextPropertyHolder(node);
  if (!parent) {
    return null;
  }

  let propValue = parent.querySelector(".ruleview-propertyvalue");
  if (!propValue) {
    return null;
  }

  return propValue.textProperty;
}

/**
 * Walker up the DOM from a given node until a parent property holder is found,
 * and return the textContent for the name and value nodes.
 * Stops at the first property found, so if node is inside the computed property
 * list, the computed property will be returned
 *
 * @param {DOMNode} node
 *        The node to start from
 * @return {Object} {name, value}
 */
function getPropertyNameAndValue(node) {
  while (true) {
    if (!node || !node.classList) {
      return null;
    }
    // Check first for ruleview-computed since it's the deepest
    if (node.classList.contains("ruleview-computed") ||
        node.classList.contains("ruleview-property")) {
      return {
        name: node.querySelector(".ruleview-propertyname").textContent,
        value: node.querySelector(".ruleview-propertyvalue").textContent
      };
    }
    node = node.parentNode;
  }
}

function RuleViewTool(inspector, window) {
  this.inspector = inspector;
  this.document = window.document;

  this.view = new CssRuleView(this.inspector, this.document);

  this.onLinkClicked = this.onLinkClicked.bind(this);
  this.onSelected = this.onSelected.bind(this);
  this.refresh = this.refresh.bind(this);
  this.clearUserProperties = this.clearUserProperties.bind(this);
  this.onPropertyChanged = this.onPropertyChanged.bind(this);
  this.onViewRefreshed = this.onViewRefreshed.bind(this);
  this.onPanelSelected = this.onPanelSelected.bind(this);
  this.onMutations = this.onMutations.bind(this);
  this.onResized = this.onResized.bind(this);

  this.view.on("ruleview-changed", this.onPropertyChanged);
  this.view.on("ruleview-refreshed", this.onViewRefreshed);
  this.view.on("ruleview-linked-clicked", this.onLinkClicked);

  this.inspector.selection.on("detached-front", this.onSelected);
  this.inspector.selection.on("new-node-front", this.onSelected);
  this.inspector.selection.on("pseudoclass", this.refresh);
  this.inspector.target.on("navigate", this.clearUserProperties);
  this.inspector.sidebar.on("ruleview-selected", this.onPanelSelected);
  this.inspector.pageStyle.on("stylesheet-updated", this.refresh);
  this.inspector.walker.on("mutations", this.onMutations);
  this.inspector.walker.on("resize", this.onResized);

  this.onSelected();
}

RuleViewTool.prototype = {
  isSidebarActive: function () {
    if (!this.view) {
      return false;
    }
    return this.inspector.sidebar.getCurrentTabID() == "ruleview";
  },

  onSelected: function (event) {
    // Ignore the event if the view has been destroyed, or if it's inactive.
    // But only if the current selection isn't null. If it's been set to null,
    // let the update go through as this is needed to empty the view on
    // navigation.
    if (!this.view) {
      return;
    }

    let isInactive = !this.isSidebarActive() &&
                     this.inspector.selection.nodeFront;
    if (isInactive) {
      return;
    }

    this.view.setPageStyle(this.inspector.pageStyle);

    if (!this.inspector.selection.isConnected() ||
        !this.inspector.selection.isElementNode()) {
      this.view.selectElement(null);
      return;
    }

    if (!event || event == "new-node-front") {
      let done = this.inspector.updating("rule-view");
      this.view.selectElement(this.inspector.selection.nodeFront)
        .then(done, done);
    }
  },

  refresh: function () {
    if (this.isSidebarActive()) {
      this.view.refreshPanel();
    }
  },

  clearUserProperties: function () {
    if (this.view && this.view.store && this.view.store.userProperties) {
      this.view.store.userProperties.clear();
    }
  },

  onPanelSelected: function () {
    if (this.inspector.selection.nodeFront === this.view._viewedElement) {
      this.refresh();
    } else {
      this.onSelected();
    }
  },

  onLinkClicked: function (e, rule) {
    let sheet = rule.parentStyleSheet;

    // Chrome stylesheets are not listed in the style editor, so show
    // these sheets in the view source window instead.
    if (!sheet || sheet.isSystem) {
      let href = rule.nodeHref || rule.href;
      let toolbox = gDevTools.getToolbox(this.inspector.target);
      toolbox.viewSource(href, rule.line);
      return;
    }

    let location = promise.resolve(rule.location);
    if (Services.prefs.getBoolPref(PREF_ORIG_SOURCES)) {
      location = rule.getOriginalLocation();
    }
    location.then(({ source, href, line, column }) => {
      let target = this.inspector.target;
      if (Tools.styleEditor.isTargetSupported(target)) {
        gDevTools.showToolbox(target, "styleeditor").then(function (toolbox) {
          let url = source || href;
          toolbox.getCurrentPanel().selectStyleSheet(url, line, column);
        });
      }
      return;
    });
  },

  onPropertyChanged: function () {
    this.inspector.markDirty();
  },

  onViewRefreshed: function () {
    this.inspector.emit("rule-view-refreshed");
  },

  /**
   * When markup mutations occur, if an attribute of the selected node changes,
   * we need to refresh the view as that might change the node's styles.
   */
  onMutations: function (mutations) {
    for (let {type, target} of mutations) {
      if (target === this.inspector.selection.nodeFront &&
          type === "attributes") {
        this.refresh();
        break;
      }
    }
  },

  /**
   * When the window gets resized, this may cause media-queries to match, and
   * therefore, different styles may apply.
   */
  onResized: function () {
    this.refresh();
  },

  destroy: function () {
    this.inspector.walker.off("mutations", this.onMutations);
    this.inspector.walker.off("resize", this.onResized);
    this.inspector.selection.off("detached-front", this.onSelected);
    this.inspector.selection.off("pseudoclass", this.refresh);
    this.inspector.selection.off("new-node-front", this.onSelected);
    this.inspector.target.off("navigate", this.clearUserProperties);
    this.inspector.sidebar.off("ruleview-selected", this.onPanelSelected);
    if (this.inspector.pageStyle) {
      this.inspector.pageStyle.off("stylesheet-updated", this.refresh);
    }

    this.view.off("ruleview-linked-clicked", this.onLinkClicked);
    this.view.off("ruleview-changed", this.onPropertyChanged);
    this.view.off("ruleview-refreshed", this.onViewRefreshed);

    this.view.destroy();

    this.view = this.document = this.inspector = null;
  }
};

exports.CssRuleView = CssRuleView;
exports.RuleViewTool = RuleViewTool;
