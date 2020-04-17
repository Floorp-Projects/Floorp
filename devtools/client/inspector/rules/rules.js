/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const promise = require("promise");
const Services = require("Services");
const flags = require("devtools/shared/flags");
const { l10n } = require("devtools/shared/inspector/css-logic");
const { PSEUDO_CLASSES } = require("devtools/shared/css/constants");
const { ELEMENT_STYLE } = require("devtools/shared/specs/styles");
const OutputParser = require("devtools/client/shared/output-parser");
const { PrefObserver } = require("devtools/client/shared/prefs");
const ElementStyle = require("devtools/client/inspector/rules/models/element-style");
const RuleEditor = require("devtools/client/inspector/rules/views/rule-editor");
const TooltipsOverlay = require("devtools/client/inspector/shared/tooltips-overlay");
const {
  createChild,
  promiseWarn,
} = require("devtools/client/inspector/shared/utils");
const { debounce } = require("devtools/shared/debounce");
const EventEmitter = require("devtools/shared/event-emitter");

loader.lazyRequireGetter(
  this,
  "flashElementOn",
  "devtools/client/inspector/markup/utils",
  true
);
loader.lazyRequireGetter(
  this,
  "flashElementOff",
  "devtools/client/inspector/markup/utils",
  true
);
loader.lazyRequireGetter(
  this,
  "ClassListPreviewer",
  "devtools/client/inspector/rules/views/class-list-previewer"
);
loader.lazyRequireGetter(
  this,
  "getNodeInfo",
  "devtools/client/inspector/rules/utils/utils",
  true
);
loader.lazyRequireGetter(
  this,
  "COLOR_SCHEMES",
  "devtools/client/inspector/rules/constants",
  true
);
loader.lazyRequireGetter(
  this,
  "StyleInspectorMenu",
  "devtools/client/inspector/shared/style-inspector-menu"
);
loader.lazyRequireGetter(
  this,
  "AutocompletePopup",
  "devtools/client/shared/autocomplete-popup"
);
loader.lazyRequireGetter(
  this,
  "KeyShortcuts",
  "devtools/client/shared/key-shortcuts"
);
loader.lazyRequireGetter(
  this,
  "clipboardHelper",
  "devtools/shared/platform/clipboard"
);

const HTML_NS = "http://www.w3.org/1999/xhtml";
const PREF_UA_STYLES = "devtools.inspector.showUserAgentStyles";
const PREF_DEFAULT_COLOR_UNIT = "devtools.defaultColorUnit";
const FILTER_CHANGED_TIMEOUT = 150;
// Removes the flash-out class from an element after 1 second.
const PROPERTY_FLASHING_DURATION = 1000;

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
 */
function CssRuleView(inspector, document, store) {
  EventEmitter.decorate(this);

  this.inspector = inspector;
  this.cssProperties = inspector.cssProperties;
  this.styleDocument = document;
  this.styleWindow = this.styleDocument.defaultView;
  this.store = store || {};

  // Allow tests to override debouncing behavior, as this can cause intermittents.
  this.debounce = debounce;

  this._outputParser = new OutputParser(document, this.cssProperties);

  this._onAddRule = this._onAddRule.bind(this);
  this._onContextMenu = this._onContextMenu.bind(this);
  this._onCopy = this._onCopy.bind(this);
  this._onFilterStyles = this._onFilterStyles.bind(this);
  this._onClearSearch = this._onClearSearch.bind(this);
  this._onTogglePseudoClassPanel = this._onTogglePseudoClassPanel.bind(this);
  this._onTogglePseudoClass = this._onTogglePseudoClass.bind(this);
  this._onToggleClassPanel = this._onToggleClassPanel.bind(this);
  this._onToggleColorSchemeSimulation = this._onToggleColorSchemeSimulation.bind(
    this
  );
  this._onTogglePrintSimulation = this._onTogglePrintSimulation.bind(this);
  this.highlightElementRule = this.highlightElementRule.bind(this);
  this.highlightProperty = this.highlightProperty.bind(this);
  this.refreshPanel = this.refreshPanel.bind(this);

  const doc = this.styleDocument;
  this.element = doc.getElementById("ruleview-container-focusable");
  this.addRuleButton = doc.getElementById("ruleview-add-rule-button");
  this.searchField = doc.getElementById("ruleview-searchbox");
  this.searchClearButton = doc.getElementById("ruleview-searchinput-clear");
  this.pseudoClassPanel = doc.getElementById("pseudo-class-panel");
  this.pseudoClassToggle = doc.getElementById("pseudo-class-panel-toggle");
  this.classPanel = doc.getElementById("ruleview-class-panel");
  this.classToggle = doc.getElementById("class-panel-toggle");
  this.colorSchemeSimulationButton = doc.getElementById(
    "color-scheme-simulation-toggle"
  );
  this.printSimulationButton = doc.getElementById("print-simulation-toggle");

  this._initSimulationFeatures();

  this.searchClearButton.hidden = true;

  this.shortcuts = new KeyShortcuts({ window: this.styleWindow });
  this._onShortcut = this._onShortcut.bind(this);
  this.shortcuts.on("Escape", event => this._onShortcut("Escape", event));
  this.shortcuts.on("Return", event => this._onShortcut("Return", event));
  this.shortcuts.on("Space", event => this._onShortcut("Space", event));
  this.shortcuts.on("CmdOrCtrl+F", event =>
    this._onShortcut("CmdOrCtrl+F", event)
  );
  this.element.addEventListener("copy", this._onCopy);
  this.element.addEventListener("contextmenu", this._onContextMenu);
  this.addRuleButton.addEventListener("click", this._onAddRule);
  this.searchField.addEventListener("input", this._onFilterStyles);
  this.searchClearButton.addEventListener("click", this._onClearSearch);
  this.pseudoClassToggle.addEventListener(
    "click",
    this._onTogglePseudoClassPanel
  );
  this.classToggle.addEventListener("click", this._onToggleClassPanel);
  // The "change" event bubbles up from checkbox inputs nested within the panel container.
  this.pseudoClassPanel.addEventListener("change", this._onTogglePseudoClass);

  if (flags.testing) {
    // In tests, we start listening immediately to avoid having to simulate a mousemove.
    this.highlighters.addToView(this);
  } else {
    this.element.addEventListener(
      "mousemove",
      () => {
        this.highlighters.addToView(this);
      },
      { once: true }
    );
  }

  this._handlePrefChange = this._handlePrefChange.bind(this);
  this._handleUAStylePrefChange = this._handleUAStylePrefChange.bind(this);
  this._handleDefaultColorUnitPrefChange = this._handleDefaultColorUnitPrefChange.bind(
    this
  );

  this._prefObserver = new PrefObserver("devtools.");
  this._prefObserver.on(PREF_UA_STYLES, this._handleUAStylePrefChange);
  this._prefObserver.on(
    PREF_DEFAULT_COLOR_UNIT,
    this._handleDefaultColorUnitPrefChange
  );

  this.pseudoClassCheckboxes = this._createPseudoClassCheckboxes();
  this.showUserAgentStyles = Services.prefs.getBoolPref(PREF_UA_STYLES);

  // Add the tooltips and highlighters to the view
  this.tooltips = new TooltipsOverlay(this);
}

CssRuleView.prototype = {
  // The element that we're inspecting.
  _viewedElement: null,

  // Used for cancelling timeouts in the style filter.
  _filterChangedTimeout: null,

  // Empty, unconnected element of the same type as this node, used
  // to figure out how shorthand properties will be parsed.
  _dummyElement: null,

  get popup() {
    if (!this._popup) {
      // The popup will be attached to the toolbox document.
      this._popup = new AutocompletePopup(this.inspector.toolbox.doc, {
        autoSelect: true,
      });
    }

    return this._popup;
  },

  get classListPreviewer() {
    if (!this._classListPreviewer) {
      this._classListPreviewer = new ClassListPreviewer(
        this.inspector,
        this.classPanel
      );
    }

    return this._classListPreviewer;
  },

  get contextMenu() {
    if (!this._contextMenu) {
      this._contextMenu = new StyleInspectorMenu(this, { isRuleView: true });
    }

    return this._contextMenu;
  },

  // Get the dummy elemenet.
  get dummyElement() {
    return this._dummyElement;
  },

  // Get the highlighters overlay from the Inspector.
  get highlighters() {
    if (!this._highlighters) {
      // highlighters is a lazy getter in the inspector.
      this._highlighters = this.inspector.highlighters;
    }

    return this._highlighters;
  },

  // Get the filter search value.
  get searchValue() {
    return this.searchField.value.toLowerCase();
  },

  get rules() {
    return this._elementStyle ? this._elementStyle.rules : [];
  },

  get currentTarget() {
    return this.inspector.toolbox.target;
  },

  /**
   * Get an instance of SelectorHighlighter (used to highlight nodes that match
   * selectors in the rule-view). A new instance is only created the first time
   * this function is called. The same instance will then be returned.
   *
   * @return {Promise} Resolves to the instance of the highlighter.
   */
  async getSelectorHighlighter() {
    if (!this.inspector) {
      return null;
    }

    if (this.selectorHighlighter) {
      return this.selectorHighlighter;
    }

    try {
      const front = this.inspector.inspectorFront;
      const h = await front.getHighlighterByType("SelectorHighlighter");
      this.selectorHighlighter = h;
      return h;
    } catch (e) {
      // The SelectorHighlighter type could not be created in the
      // current target.  It could be an older server, or a XUL page.
      return null;
    }
  },

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
  async toggleSelectorHighlighter(selectorIcon, selector) {
    if (this.lastSelectorIcon) {
      this.lastSelectorIcon.classList.remove("highlighted");
    }
    selectorIcon.classList.remove("highlighted");

    const highlighter = await this.getSelectorHighlighter();
    if (!highlighter) {
      return;
    }

    await highlighter.hide();

    if (selector !== this.highlighters.selectorHighlighterShown) {
      this.highlighters.selectorHighlighterShown = selector;
      selectorIcon.classList.add("highlighted");
      this.lastSelectorIcon = selectorIcon;

      const node = this.inspector.selection.nodeFront;

      await highlighter.show(node, {
        hideInfoBar: true,
        hideGuides: true,
        selector,
      });

      this.emit("ruleview-selectorhighlighter-toggled", true);
    } else {
      this.highlighters.selectorHighlighterShown = null;
      this.emit("ruleview-selectorhighlighter-toggled", false);
    }
  },

  isPanelVisible: function() {
    if (this.inspector.is3PaneModeEnabled) {
      return true;
    }
    return (
      this.inspector.toolbox &&
      this.inspector.sidebar &&
      this.inspector.toolbox.currentToolId === "inspector" &&
      this.inspector.sidebar.getCurrentTabID() == "ruleview"
    );
  },

  /**
   * Initializes the content-viewer front and enable the print and color scheme simulation
   * if they are supported in the current target.
   */
  async _initSimulationFeatures() {
    // In order to query if the content-viewer actor's print and color simulation methods are
    // supported, we have to call the content-viewer front so that the actor is lazily loaded.
    // This allows us to use `actorHasMethod`. Please see `getActorDescription` for more
    // information.
    this.contentViewerFront = await this.currentTarget.getFront(
      "contentViewer"
    );

    if (!this.currentTarget.chrome) {
      this.printSimulationButton.removeAttribute("hidden");
      this.printSimulationButton.addEventListener(
        "click",
        this._onTogglePrintSimulation
      );
    }

    // Show the color scheme simulation toggle button if:
    // - The feature pref is enabled.
    // - Color scheme simulation is supported for the current target.
    const isEmulateColorSchemeSupported = await this.currentTarget.actorHasMethod(
      "contentViewer",
      "getEmulatedColorScheme"
    );

    if (
      Services.prefs.getBoolPref(
        "devtools.inspector.color-scheme-simulation.enabled"
      ) &&
      isEmulateColorSchemeSupported
    ) {
      this.colorSchemeSimulationButton.removeAttribute("hidden");
      this.colorSchemeSimulationButton.addEventListener(
        "click",
        this._onToggleColorSchemeSimulation
      );
    }
  },

  /**
   * Get the type of a given node in the rule-view
   *
   * @param {DOMNode} node
   *        The node which we want information about
   * @return {Object|null} containing the following props:
   * - type {String} One of the VIEW_NODE_XXX_TYPE const in
   *   client/inspector/shared/node-types.
   * - rule {Rule} The Rule object.
   * - value {Object} Depends on the type of the node.
   * Otherwise, returns null if the node isn't anything we care about.
   */
  getNodeInfo: function(node) {
    return getNodeInfo(node, this._elementStyle);
  },

  /**
   * Context menu handler.
   */
  _onContextMenu: function(event) {
    if (
      event.originalTarget.closest("input[type=text]") ||
      event.originalTarget.closest("input:not([type])") ||
      event.originalTarget.closest("textarea")
    ) {
      return;
    }

    event.stopPropagation();
    event.preventDefault();

    this.contextMenu.show(event);
  },

  /**
   * Callback for copy event. Copy the selected text.
   *
   * @param {Event} event
   *        copy event object.
   */
  _onCopy: function(event) {
    if (event) {
      this.copySelection(event.target);
      event.preventDefault();
      event.stopPropagation();
    }
  },

  /**
   * Copy the current selection. The current target is necessary
   * if the selection is inside an input or a textarea
   *
   * @param {DOMNode} target
   *        DOMNode target of the copy action
   */
  copySelection: function(target) {
    try {
      let text = "";

      const nodeName = target?.nodeName;
      if (nodeName === "input" || nodeName == "textarea") {
        const start = Math.min(target.selectionStart, target.selectionEnd);
        const end = Math.max(target.selectionStart, target.selectionEnd);
        const count = end - start;
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
   * Add a new rule to the current element.
   */
  _onAddRule: function() {
    const elementStyle = this._elementStyle;
    const element = elementStyle.element;
    const pseudoClasses = element.pseudoClassLocks;

    // Adding a new rule with authored styles will cause the actor to
    // emit an event, which will in turn cause the rule view to be
    // updated.  So, we wait for this update and for the rule creation
    // request to complete, and then focus the new rule's selector.
    const eventPromise = this.once("ruleview-refreshed");
    const newRulePromise = this.pageStyle.addNewRule(element, pseudoClasses);
    promise.all([eventPromise, newRulePromise]).then(values => {
      const options = values[1];
      // Be sure the reference the correct |rules| here.
      for (const rule of this._elementStyle.rules) {
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
  refreshAddRuleButtonState: function() {
    const shouldBeDisabled =
      !this._viewedElement ||
      !this.inspector.selection.isElementNode() ||
      this.inspector.selection.isAnonymousNode();
    this.addRuleButton.disabled = shouldBeDisabled;
  },

  /**
   * Return {Boolean} true if the rule view currently has an input
   * editor visible.
   */
  get isEditing() {
    return (
      this.tooltips.isEditing ||
      this.element.querySelectorAll(".styleinspector-propertyeditor").length > 0
    );
  },

  _handleUAStylePrefChange: function() {
    this.showUserAgentStyles = Services.prefs.getBoolPref(PREF_UA_STYLES);
    this._handlePrefChange(PREF_UA_STYLES);
  },

  _handleDefaultColorUnitPrefChange: function() {
    this._handlePrefChange(PREF_DEFAULT_COLOR_UNIT);
  },

  _handlePrefChange: function(pref) {
    // Reselect the currently selected element
    const refreshOnPrefs = [PREF_UA_STYLES, PREF_DEFAULT_COLOR_UNIT];
    if (refreshOnPrefs.indexOf(pref) > -1) {
      this.selectElement(this._viewedElement, true);
    }
  },

  /**
   * Set the filter style search value.
   * @param {String} value
   *        The search value.
   */
  setFilterStyles: function(value = "") {
    this.searchField.value = value;
    this.searchField.focus();
    this._onFilterStyles();
  },

  /**
   * Called when the user enters a search term in the filter style search box.
   */
  _onFilterStyles: function() {
    if (this._filterChangedTimeout) {
      clearTimeout(this._filterChangedTimeout);
    }

    const filterTimeout =
      this.searchValue.length > 0 ? FILTER_CHANGED_TIMEOUT : 0;
    this.searchClearButton.hidden = this.searchValue.length === 0;

    this._filterChangedTimeout = setTimeout(() => {
      this.searchData = {
        searchPropertyMatch: FILTER_PROP_RE.exec(this.searchValue),
        searchPropertyName: this.searchValue,
        searchPropertyValue: this.searchValue,
        strictSearchValue: "",
        strictSearchPropertyName: false,
        strictSearchPropertyValue: false,
        strictSearchAllValues: false,
      };

      if (this.searchData.searchPropertyMatch) {
        // Parse search value as a single property line and extract the
        // property name and value. If the parsed property name or value is
        // contained in backquotes (`), extract the value within the backquotes
        // and set the corresponding strict search for the property to true.
        if (FILTER_STRICT_RE.test(this.searchData.searchPropertyMatch[1])) {
          this.searchData.strictSearchPropertyName = true;
          this.searchData.searchPropertyName = FILTER_STRICT_RE.exec(
            this.searchData.searchPropertyMatch[1]
          )[1];
        } else {
          this.searchData.searchPropertyName = this.searchData.searchPropertyMatch[1];
        }

        if (FILTER_STRICT_RE.test(this.searchData.searchPropertyMatch[2])) {
          this.searchData.strictSearchPropertyValue = true;
          this.searchData.searchPropertyValue = FILTER_STRICT_RE.exec(
            this.searchData.searchPropertyMatch[2]
          )[1];
        } else {
          this.searchData.searchPropertyValue = this.searchData.searchPropertyMatch[2];
        }

        // Strict search for stylesheets will match the property line regex.
        // Extract the search value within the backquotes to be used
        // in the strict search for stylesheets in _highlightStyleSheet.
        if (FILTER_STRICT_RE.test(this.searchValue)) {
          this.searchData.strictSearchValue = FILTER_STRICT_RE.exec(
            this.searchValue
          )[1];
        }
      } else if (FILTER_STRICT_RE.test(this.searchValue)) {
        // If the search value does not correspond to a property line and
        // is contained in backquotes, extract the search value within the
        // backquotes and set the flag to perform a strict search for all
        // the values (selector, stylesheet, property and computed values).
        const searchValue = FILTER_STRICT_RE.exec(this.searchValue)[1];
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
   * Called when the user clicks on the clear button in the filter style search
   * box. Returns true if the search box is cleared and false otherwise.
   */
  _onClearSearch: function() {
    if (this.searchField.value) {
      this.setFilterStyles("");
      return true;
    }

    return false;
  },

  destroy: function() {
    this.isDestroyed = true;
    this.clear();

    this._dummyElement = null;
    // off handlers must have the same reference as their on handlers
    this._prefObserver.off(PREF_UA_STYLES, this._handleUAStylePrefChange);
    this._prefObserver.off(
      PREF_DEFAULT_COLOR_UNIT,
      this._handleDefaultColorUnitPrefChange
    );
    this._prefObserver.destroy();

    this._outputParser = null;

    if (this._classListPreviewer) {
      this._classListPreviewer.destroy();
      this._classListPreviewer = null;
    }

    if (this._contextMenu) {
      this._contextMenu.destroy();
      this._contextMenu = null;
    }

    if (this._highlighters) {
      this._highlighters.removeFromView(this);
      this._highlighters = null;
    }

    // Clean-up for print simulation.
    if (this.contentViewerFront) {
      this.colorSchemeSimulationButton.removeEventListener(
        "click",
        this._onToggleColorSchemeSimulation
      );
      this.printSimulationButton.removeEventListener(
        "click",
        this._onTogglePrintSimulation
      );

      this.contentViewerFront.destroy();

      this.colorSchemeSimulationButton = null;
      this.printSimulationButton = null;
      this.contentViewerFront = null;
    }

    this.tooltips.destroy();

    // Remove bound listeners
    this.shortcuts.destroy();
    this.element.removeEventListener("copy", this._onCopy);
    this.element.removeEventListener("contextmenu", this._onContextMenu);
    this.addRuleButton.removeEventListener("click", this._onAddRule);
    this.searchField.removeEventListener("input", this._onFilterStyles);
    this.searchClearButton.removeEventListener("click", this._onClearSearch);
    this.pseudoClassPanel.removeEventListener(
      "change",
      this._onTogglePseudoClass
    );
    this.pseudoClassToggle.removeEventListener(
      "click",
      this._onTogglePseudoClassPanel
    );
    this.classToggle.removeEventListener("click", this._onToggleClassPanel);

    this.searchField = null;
    this.searchClearButton = null;
    this.pseudoClassPanel = null;
    this.pseudoClassToggle = null;
    this.pseudoClassCheckboxes = null;
    this.classPanel = null;
    this.classToggle = null;

    this.inspector = null;
    this.styleDocument = null;
    this.styleWindow = null;

    if (this.element.parentNode) {
      this.element.remove();
    }

    if (this._elementStyle) {
      this._elementStyle.destroy();
    }

    if (this._popup) {
      this._popup.destroy();
      this._popup = null;
    }
  },

  /**
   * Mark the view as selecting an element, disabling all interaction, and
   * visually clearing the view after a few milliseconds to avoid confusion
   * about which element's styles the rule view shows.
   */
  _startSelectingElement: function() {
    this.element.classList.add("non-interactive");
  },

  /**
   * Mark the view as no longer selecting an element, re-enabling interaction.
   */
  _stopSelectingElement: function() {
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
  selectElement: function(element, allowRefresh = false) {
    const refresh = this._viewedElement === element;
    if (refresh && !allowRefresh) {
      return promise.resolve(undefined);
    }

    if (this._popup && this.popup.isOpen) {
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
      if (this.pageStyle) {
        this.pageStyle.off("stylesheet-updated", this.refreshPanel);
        this.pageStyle = null;
      }
      return promise.resolve(undefined);
    }

    this.pageStyle = element.inspectorFront.pageStyle;
    this.pageStyle.on("stylesheet-updated", this.refreshPanel);

    // To figure out how shorthand properties are interpreted by the
    // engine, we will set properties on a dummy element and observe
    // how their .style attribute reflects them as computed values.
    const dummyElementPromise = promise
      .resolve(this.styleDocument)
      .then(document => {
        // ::before and ::after do not have a namespaceURI
        const namespaceURI =
          this.element.namespaceURI || document.documentElement.namespaceURI;
        this._dummyElement = document.createElementNS(
          namespaceURI,
          this.element.tagName
        );
      })
      .catch(promiseWarn);

    const elementStyle = new ElementStyle(
      element,
      this,
      this.store,
      this.pageStyle,
      this.showUserAgentStyles
    );
    this._elementStyle = elementStyle;

    this._startSelectingElement();

    return dummyElementPromise
      .then(() => {
        if (this._elementStyle === elementStyle) {
          return this._populate();
        }
        return undefined;
      })
      .then(() => {
        if (this._elementStyle === elementStyle) {
          if (!refresh) {
            this.element.scrollTop = 0;
          }
          this._stopSelectingElement();
          this._elementStyle.onChanged = () => {
            this._changed();
          };
        }
      })
      .catch(e => {
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
  refreshPanel: function() {
    // Ignore refreshes when the panel is hidden, or during editing or when no element is selected.
    if (!this.isPanelVisible() || this.isEditing || !this._elementStyle) {
      return promise.resolve(undefined);
    }

    // Repopulate the element style once the current modifications are done.
    const promises = [];
    for (const rule of this._elementStyle.rules) {
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
  clearPseudoClassPanel: function() {
    this.pseudoClassCheckboxes.forEach(checkbox => {
      checkbox.checked = false;
      checkbox.disabled = false;
    });
  },

  /**
   * For each item in PSEUDO_CLASSES, create a checkbox input element for toggling a
   * pseudo-class on the selected element and append it to the pseudo-class panel.
   *
   * Returns an array with the checkbox input elements for pseudo-classes.
   *
   * @return {Array}
   */
  _createPseudoClassCheckboxes: function() {
    const doc = this.styleDocument;
    const fragment = doc.createDocumentFragment();

    for (const pseudo of PSEUDO_CLASSES) {
      const label = doc.createElement("label");
      const checkbox = doc.createElement("input");
      checkbox.setAttribute("tabindex", "-1");
      checkbox.setAttribute("type", "checkbox");
      checkbox.setAttribute("value", pseudo);

      label.append(checkbox, pseudo);
      fragment.append(label);
    }

    this.pseudoClassPanel.append(fragment);
    return Array.from(
      this.pseudoClassPanel.querySelectorAll("input[type=checkbox]")
    );
  },

  /**
   * Update the pseudo class options for the currently highlighted element.
   */
  refreshPseudoClassPanel: function() {
    if (!this._elementStyle || !this.inspector.selection.isElementNode()) {
      this.pseudoClassCheckboxes.forEach(checkbox => {
        checkbox.disabled = true;
      });
      return;
    }

    const pseudoClassLocks = this._elementStyle.element.pseudoClassLocks;
    this.pseudoClassCheckboxes.forEach(checkbox => {
      checkbox.disabled = false;
      checkbox.checked = pseudoClassLocks.includes(checkbox.value);
    });
  },

  _populate: function() {
    const elementStyle = this._elementStyle;
    return this._elementStyle
      .populate()
      .then(() => {
        if (this._elementStyle !== elementStyle || this.isDestroyed) {
          return null;
        }

        this._clearRules();
        const onEditorsReady = this._createEditors();
        this.refreshPseudoClassPanel();

        // Notify anyone that cares that we refreshed.
        return onEditorsReady.then(() => {
          this.emit("ruleview-refreshed");
        }, console.error);
      })
      .catch(promiseWarn);
  },

  /**
   * Show the user that the rule view has no node selected.
   */
  _showEmpty: function() {
    if (this.styleDocument.getElementById("ruleview-no-results")) {
      return;
    }

    createChild(this.element, "div", {
      id: "ruleview-no-results",
      class: "devtools-sidepanel-no-result",
      textContent: l10n("rule.empty"),
    });
  },

  /**
   * Clear the rules.
   */
  _clearRules: function() {
    this.element.innerHTML = "";
  },

  /**
   * Clear the rule view.
   */
  clear: function(clearDom = true) {
    this.lastSelectorIcon = null;

    if (clearDom) {
      this._clearRules();
    }
    this._viewedElement = null;

    if (this._elementStyle) {
      this._elementStyle.destroy();
      this._elementStyle = null;
    }

    if (this.pageStyle) {
      this.pageStyle.off("stylesheet-updated", this.refreshPanel);
      this.pageStyle = null;
    }
  },

  /**
   * Called when the user has made changes to the ElementStyle.
   * Emits an event that clients can listen to.
   */
  _changed: function() {
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
      this._showPseudoElements = Services.prefs.getBoolPref(
        "devtools.inspector.show_pseudo_elements"
      );
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
  createExpandableContainer: function(label, isPseudo = false) {
    const header = this.styleDocument.createElementNS(HTML_NS, "div");
    header.className = this._getRuleViewHeaderClassName(true);
    header.textContent = label;

    const twisty = this.styleDocument.createElementNS(HTML_NS, "span");
    twisty.className = "ruleview-expander theme-twisty";
    twisty.setAttribute("open", "true");

    header.insertBefore(twisty, header.firstChild);
    this.element.appendChild(header);

    const container = this.styleDocument.createElementNS(HTML_NS, "div");
    container.classList.add("ruleview-expandable-container");
    container.hidden = false;
    this.element.appendChild(container);

    header.addEventListener("click", () => {
      this._toggleContainerVisibility(
        twisty,
        container,
        isPseudo,
        !this.showPseudoElements
      );
    });

    if (isPseudo) {
      container.id = "pseudo-elements-container";
      twisty.id = "pseudo-elements-header-twisty";
      this._toggleContainerVisibility(
        twisty,
        container,
        isPseudo,
        this.showPseudoElements
      );
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
  _toggleContainerVisibility: function(
    twisty,
    container,
    isPseudo,
    showPseudo
  ) {
    let isOpen = twisty.getAttribute("open");

    if (isPseudo) {
      this._showPseudoElements = !!showPseudo;

      Services.prefs.setBoolPref(
        "devtools.inspector.show_pseudo_elements",
        this.showPseudoElements
      );

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

  _getRuleViewHeaderClassName: function(isPseudo) {
    const baseClassName = "ruleview-header";
    return isPseudo
      ? baseClassName + " ruleview-expandable-header"
      : baseClassName;
  },

  /**
   * Creates editor UI for each of the rules in _elementStyle.
   */
  // eslint-disable-next-line complexity
  _createEditors: function() {
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

    const editorReadyPromises = [];
    for (const rule of this._elementStyle.rules) {
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
        const div = this.styleDocument.createElementNS(HTML_NS, "div");
        div.className = this._getRuleViewHeaderClassName();
        div.textContent = this.selectedElementLabel;
        this.element.appendChild(div);
      }

      const inheritedSource = rule.inherited;
      if (inheritedSource && inheritedSource !== lastInheritedSource) {
        const div = this.styleDocument.createElementNS(HTML_NS, "div");
        div.className = this._getRuleViewHeaderClassName();
        div.textContent = rule.inheritedSource;
        lastInheritedSource = inheritedSource;
        this.element.appendChild(div);
      }

      if (!seenPseudoElement && rule.pseudoElement) {
        seenPseudoElement = true;
        container = this.createExpandableContainer(
          this.pseudoElementLabel,
          true
        );
      }

      const keyframes = rule.keyframes;
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

    const searchBox = this.searchField.parentNode;
    searchBox.classList.toggle(
      "devtools-searchbox-no-match",
      this.searchValue && !seenSearchTerm
    );

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
  highlightRule: function(rule) {
    const isRuleSelectorHighlighted = this._highlightRuleSelector(rule);
    const isStyleSheetHighlighted = this._highlightStyleSheet(rule);
    let isHighlighted = isRuleSelectorHighlighted || isStyleSheetHighlighted;

    // Highlight search matches in the rule properties
    for (const textProp of rule.textProps) {
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
  _highlightRuleSelector: function(rule) {
    let isSelectorHighlighted = false;

    let selectorNodes = [...rule.editor.selectorText.childNodes];
    if (rule.domRule.type === CSSRule.KEYFRAME_RULE) {
      selectorNodes = [rule.editor.selectorText];
    } else if (rule.domRule.type === ELEMENT_STYLE) {
      selectorNodes = [];
    }

    // Highlight search matches in the rule selectors
    for (const selectorNode of selectorNodes) {
      const selector = selectorNode.textContent.toLowerCase();
      if (
        (this.searchData.strictSearchAllValues &&
          selector === this.searchData.strictSearchValue) ||
        (!this.searchData.strictSearchAllValues &&
          selector.includes(this.searchValue))
      ) {
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
  _highlightStyleSheet: function(rule) {
    const styleSheetSource = rule.title.toLowerCase();
    const isStyleSheetHighlighted = this.searchData.strictSearchValue
      ? styleSheetSource === this.searchData.strictSearchValue
      : styleSheetSource.includes(this.searchValue);

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
  _highlightProperty: function(editor) {
    const isPropertyHighlighted = this._highlightRuleProperty(editor);
    const isComputedHighlighted = this._highlightComputedProperty(editor);

    // Expand the computed list if a computed property is highlighted and the
    // property rule is not highlighted
    if (
      !isPropertyHighlighted &&
      isComputedHighlighted &&
      !editor.computed.hasAttribute("user-open")
    ) {
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
  _updatePropertyHighlight: function(editor) {
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
  _highlightRuleProperty: function(editor) {
    // Get the actual property value displayed in the rule view
    const propertyName = editor.prop.name.toLowerCase();
    const propertyValue = editor.valueSpan.textContent.toLowerCase();

    return this._highlightMatches(
      editor.container,
      propertyName,
      propertyValue
    );
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
  _highlightComputedProperty: function(editor) {
    let isComputedHighlighted = false;

    // Highlight search matches in the computed list of properties
    editor._populateComputed();
    for (const computed of editor.prop.computed) {
      if (computed.element) {
        // Get the actual property value displayed in the computed list
        const computedName = computed.name.toLowerCase();
        const computedValue = computed.parsedValue.toLowerCase();

        isComputedHighlighted = this._highlightMatches(
          computed.element,
          computedName,
          computedValue
        )
          ? true
          : isComputedHighlighted;
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
  _highlightMatches: function(element, propertyName, propertyValue) {
    const {
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
    const hasNameAndValue =
      searchPropertyMatch && searchPropertyName && searchPropertyValue;
    const isMatch = (value, query, isStrict) => {
      return isStrict ? value === query : query && value.includes(query);
    };

    if (hasNameAndValue) {
      matches =
        isMatch(propertyName, searchPropertyName, strictSearchPropertyName) &&
        isMatch(propertyValue, searchPropertyValue, strictSearchPropertyValue);
    } else {
      matches =
        isMatch(
          propertyName,
          searchPropertyName,
          strictSearchPropertyName || strictSearchAllValues
        ) ||
        isMatch(
          propertyValue,
          searchPropertyValue,
          strictSearchPropertyValue || strictSearchAllValues
        );
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
  _clearHighlight: function(element) {
    for (const el of element.querySelectorAll(".ruleview-highlight")) {
      el.classList.remove("ruleview-highlight");
    }

    for (const computed of element.querySelectorAll(
      ".ruleview-computedlist[filter-open]"
    )) {
      computed.parentNode._textPropertyEditor.collapseForFilter();
    }
  },

  /**
   * Called when the pseudo class panel button is clicked and toggles
   * the display of the pseudo class panel.
   */
  _onTogglePseudoClassPanel: function() {
    if (this.pseudoClassPanel.hidden) {
      this.showPseudoClassPanel();
    } else {
      this.hidePseudoClassPanel();
    }
  },

  showPseudoClassPanel: function() {
    this.hideClassPanel();

    this.pseudoClassToggle.classList.add("checked");
    this.pseudoClassCheckboxes.forEach(checkbox => {
      checkbox.setAttribute("tabindex", "0");
    });
    this.pseudoClassPanel.hidden = false;
  },

  hidePseudoClassPanel: function() {
    this.pseudoClassToggle.classList.remove("checked");
    this.pseudoClassCheckboxes.forEach(checkbox => {
      checkbox.setAttribute("tabindex", "-1");
    });
    this.pseudoClassPanel.hidden = true;
  },

  /**
   * Called when a pseudo class checkbox is clicked and toggles
   * the pseudo class for the current selected element.
   */
  _onTogglePseudoClass: function(event) {
    const target = event.target;
    this.inspector.togglePseudoClass(target.value);
  },

  /**
   * Called when the class panel button is clicked and toggles the display of the class
   * panel.
   */
  _onToggleClassPanel: function() {
    if (this.classPanel.hidden) {
      this.showClassPanel();
    } else {
      this.hideClassPanel();
    }
  },

  showClassPanel: function() {
    this.hidePseudoClassPanel();

    this.classToggle.classList.add("checked");
    this.classPanel.hidden = false;

    this.classListPreviewer.focusAddClassField();
  },

  hideClassPanel: function() {
    this.classToggle.classList.remove("checked");
    this.classPanel.hidden = true;
  },

  /**
   * Handle the keypress event in the rule view.
   */
  _onShortcut: function(name, event) {
    if (!event.target.closest("#sidebar-panel-ruleview")) {
      return;
    }

    if (name === "CmdOrCtrl+F") {
      this.searchField.focus();
      event.preventDefault();
    } else if (
      (name === "Return" || name === "Space") &&
      this.element.classList.contains("non-interactive")
    ) {
      event.preventDefault();
    } else if (
      name === "Escape" &&
      event.target === this.searchField &&
      this._onClearSearch()
    ) {
      // Handle the search box's keypress event. If the escape key is pressed,
      // clear the search box field.
      event.preventDefault();
      event.stopPropagation();
    }
  },

  async _onToggleColorSchemeSimulation() {
    const currentState = await this.contentViewerFront.getEmulatedColorScheme();
    const index = COLOR_SCHEMES.indexOf(currentState);
    const nextState = COLOR_SCHEMES[(index + 1) % COLOR_SCHEMES.length];

    if (nextState) {
      this.colorSchemeSimulationButton.setAttribute("state", nextState);
    } else {
      this.colorSchemeSimulationButton.removeAttribute("state");
    }

    await this.contentViewerFront.setEmulatedColorScheme(nextState);
    this.refreshPanel();
  },

  async _onTogglePrintSimulation() {
    const enabled = await this.contentViewerFront.getIsPrintSimulationEnabled();

    if (!enabled) {
      this.printSimulationButton.classList.add("checked");
      await this.contentViewerFront.startPrintMediaSimulation();
    } else {
      this.printSimulationButton.classList.remove("checked");
      await this.contentViewerFront.stopPrintMediaSimulation(false);
    }

    // Refresh the current element's rules in the panel.
    this.refreshPanel();
  },

  /**
   * Temporarily flash the given element.
   *
   * @param  {Element} element
   *         The element.
   */
  _flashElement(element) {
    flashElementOn(element, {
      backgroundClass: "theme-bg-contrast",
    });

    if (this._flashMutationTimer) {
      clearTimeout(this._removeFlashOutTimer);
      this._flashMutationTimer = null;
    }

    this._flashMutationTimer = setTimeout(() => {
      flashElementOff(element, {
        backgroundClass: "theme-bg-contrast",
      });

      // Emit "scrolled-to-property" for use by tests.
      this.emit("scrolled-to-element");
    }, PROPERTY_FLASHING_DURATION);
  },

  /**
   * Scrolls to the top of either the rule or declaration. The view will try to scroll to
   * the rule if both can fit in the viewport. If not, then scroll to the declaration.
   *
   * @param  {Element} rule
   *         The rule to scroll to.
   * @param  {Element|null} declaration
   *         Optional. The declaration to scroll to.
   * @param  {String} scrollBehavior
   *         Optional. The transition animation when scrolling. If prefers-reduced-motion
   *         system pref is set, then the scroll behavior will be overridden to "auto".
   */
  _scrollToElement(rule, declaration, scrollBehavior = "smooth") {
    let elementToScrollTo = rule;

    if (declaration) {
      const { offsetTop, offsetHeight } = declaration;
      // Get the distance between both the rule and declaration. If the distance is
      // greater than the height of the rule view, then only scroll to the declaration.
      const distance = offsetTop + offsetHeight - rule.offsetTop;

      if (this.element.parentNode.offsetHeight <= distance) {
        elementToScrollTo = declaration;
      }
    }

    // Ensure that smooth scrolling is disabled when the user prefers reduced motion.
    const win = elementToScrollTo.ownerGlobal;
    const reducedMotion = win.matchMedia("(prefers-reduced-motion)").matches;
    scrollBehavior = reducedMotion ? "auto" : scrollBehavior;

    elementToScrollTo.scrollIntoView({ behavior: scrollBehavior });
  },

  /**
   * Toggles the visibility of the pseudo element rule's container.
   */
  _togglePseudoElementRuleContainer() {
    const container = this.styleDocument.getElementById(
      "pseudo-elements-container"
    );
    const twisty = this.styleDocument.getElementById(
      "pseudo-elements-header-twisty"
    );
    this._toggleContainerVisibility(twisty, container, true, true);
  },

  /**
   * Finds the rule with the matching actorID and highlights it.
   *
   * @param  {String} ruleId
   *         The actorID of the rule.
   */
  highlightElementRule: function(ruleId) {
    let scrollBehavior = "smooth";

    const rule = this.rules.find(r => r.domRule.actorID === ruleId);

    if (!rule) {
      return;
    }

    if (rule.domRule.actorID === ruleId) {
      // If using 2-Pane mode, then switch to the Rules tab first.
      if (!this.inspector.is3PaneModeEnabled) {
        this.inspector.sidebar.select("ruleview");
      }

      if (rule.pseudoElement.length && !this.showPseudoElements) {
        scrollBehavior = "auto";
        this._togglePseudoElementRuleContainer();
      }

      const {
        editor: { element },
      } = rule;

      // Scroll to the top of the rule and highlight it.
      this._scrollToElement(element, null, scrollBehavior);
      this._flashElement(element);
    }
  },

  /**
   * Finds the specified TextProperty name in the rule view. If found, scroll to and
   * flash the TextProperty.
   *
   * @param  {String} name
   *         The property name to scroll to and highlight.
   * @return {Boolean} true if the TextProperty name is found, and false otherwise.
   */
  highlightProperty: function(name) {
    for (const rule of this.rules) {
      for (const textProp of rule.textProps) {
        if (textProp.overridden || textProp.invisible || !textProp.enabled) {
          continue;
        }

        const {
          editor: { selectorText },
        } = rule;
        let scrollBehavior = "smooth";

        // First, search for a matching authored property.
        if (textProp.name === name) {
          // If using 2-Pane mode, then switch to the Rules tab first.
          if (!this.inspector.is3PaneModeEnabled) {
            this.inspector.sidebar.select("ruleview");
          }

          // If the property is being applied by a pseudo element rule, expand the pseudo
          // element list container.
          if (rule.pseudoElement.length && !this.showPseudoElements) {
            // Set the scroll behavior to "auto" to avoid timing issues between toggling
            // the pseudo element container and scrolling smoothly to the rule.
            scrollBehavior = "auto";
            this._togglePseudoElementRuleContainer();
          }

          // Scroll to the top of the property's rule so that both the property and its
          // rule are visible.
          this._scrollToElement(
            selectorText,
            textProp.editor.element,
            scrollBehavior
          );
          this._flashElement(textProp.editor.element);

          return true;
        }

        // If there is no matching property, then look in computed properties.
        for (const computed of textProp.computed) {
          if (computed.overridden) {
            continue;
          }

          if (computed.name === name) {
            if (!this.inspector.is3PaneModeEnabled) {
              this.inspector.sidebar.select("ruleview");
            }

            if (
              textProp.rule.pseudoElement.length &&
              !this.showPseudoElements
            ) {
              scrollBehavior = "auto";
              this._togglePseudoElementRuleContainer();
            }

            // Expand the computed list.
            textProp.editor.expandForFilter();

            this._scrollToElement(
              selectorText,
              computed.element,
              scrollBehavior
            );
            this._flashElement(computed.element);

            return true;
          }
        }
      }
    }

    return false;
  },
};

function RuleViewTool(inspector, window) {
  this.inspector = inspector;
  this.document = window.document;

  this.view = new CssRuleView(this.inspector, this.document);

  this.clearUserProperties = this.clearUserProperties.bind(this);
  this.refresh = this.refresh.bind(this);
  this.onDetachedFront = this.onDetachedFront.bind(this);
  this.onPanelSelected = this.onPanelSelected.bind(this);
  this.onDetachedFront = this.onDetachedFront.bind(this);
  this.onSelected = this.onSelected.bind(this);
  this.onViewRefreshed = this.onViewRefreshed.bind(this);

  this.view.on("ruleview-refreshed", this.onViewRefreshed);
  this.inspector.selection.on("detached-front", this.onDetachedFront);
  this.inspector.selection.on("new-node-front", this.onSelected);
  this.inspector.selection.on("pseudoclass", this.refresh);
  this.inspector.currentTarget.on("navigate", this.clearUserProperties);
  this.inspector.ruleViewSideBar.on("ruleview-selected", this.onPanelSelected);
  this.inspector.sidebar.on("ruleview-selected", this.onPanelSelected);
  this.inspector.styleChangeTracker.on("style-changed", this.refresh);

  this.onSelected();
}

RuleViewTool.prototype = {
  isPanelVisible: function() {
    if (!this.view) {
      return false;
    }
    return this.view.isPanelVisible();
  },

  onDetachedFront: function() {
    this.onSelected(false);
  },

  onSelected: function(selectElement = true) {
    // Ignore the event if the view has been destroyed, or if it's inactive.
    // But only if the current selection isn't null. If it's been set to null,
    // let the update go through as this is needed to empty the view on
    // navigation.
    if (!this.view) {
      return;
    }

    const isInactive =
      !this.isPanelVisible() && this.inspector.selection.nodeFront;
    if (isInactive) {
      return;
    }

    if (
      !this.inspector.selection.isConnected() ||
      !this.inspector.selection.isElementNode()
    ) {
      this.view.selectElement(null);
      return;
    }

    if (selectElement) {
      const done = this.inspector.updating("rule-view");
      this.view.selectElement(this.inspector.selection.nodeFront).finally(done);
    }
  },

  refresh: function() {
    if (this.isPanelVisible()) {
      this.view.refreshPanel();
    }
  },

  clearUserProperties: function() {
    if (this.view && this.view.store && this.view.store.userProperties) {
      this.view.store.userProperties.clear();
    }
  },

  onPanelSelected: function() {
    if (this.inspector.selection.nodeFront === this.view._viewedElement) {
      this.refresh();
    } else {
      this.onSelected();
    }
  },

  onViewRefreshed: function() {
    this.inspector.emit("rule-view-refreshed");
  },

  destroy: function() {
    this.inspector.styleChangeTracker.off("style-changed", this.refresh);
    this.inspector.selection.off("detached-front", this.onDetachedFront);
    this.inspector.selection.off("pseudoclass", this.refresh);
    this.inspector.selection.off("new-node-front", this.onSelected);
    this.inspector.currentTarget.off("navigate", this.clearUserProperties);
    this.inspector.sidebar.off("ruleview-selected", this.onPanelSelected);

    this.view.off("ruleview-refreshed", this.onViewRefreshed);

    this.view.destroy();

    this.view = this.document = this.inspector = null;
  },
};

exports.CssRuleView = CssRuleView;
exports.RuleViewTool = RuleViewTool;
