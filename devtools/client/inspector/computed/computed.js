/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const ToolDefinitions = require("devtools/client/definitions").Tools;
const CssLogic = require("devtools/shared/inspector/css-logic");
const {ELEMENT_STYLE} = require("devtools/shared/specs/styles");
const promise = require("promise");
const OutputParser = require("devtools/client/shared/output-parser");
const {PrefObserver} = require("devtools/client/shared/prefs");
const {createChild} = require("devtools/client/inspector/shared/utils");
const {gDevTools} = require("devtools/client/framework/devtools");
const {getCssProperties} = require("devtools/shared/fronts/css-properties");
const {
  VIEW_NODE_SELECTOR_TYPE,
  VIEW_NODE_PROPERTY_TYPE,
  VIEW_NODE_VALUE_TYPE,
  VIEW_NODE_IMAGE_URL_TYPE,
  VIEW_NODE_FONT_TYPE,
} = require("devtools/client/inspector/shared/node-types");
const TooltipsOverlay = require("devtools/client/inspector/shared/tooltips-overlay");

loader.lazyRequireGetter(this, "StyleInspectorMenu", "devtools/client/inspector/shared/style-inspector-menu");
loader.lazyRequireGetter(this, "KeyShortcuts", "devtools/client/shared/key-shortcuts");
loader.lazyRequireGetter(this, "clipboardHelper", "devtools/shared/platform/clipboard");

const STYLE_INSPECTOR_PROPERTIES = "devtools/shared/locales/styleinspector.properties";
const {LocalizationHelper} = require("devtools/shared/l10n");
const STYLE_INSPECTOR_L10N = new LocalizationHelper(STYLE_INSPECTOR_PROPERTIES);

const FILTER_CHANGED_TIMEOUT = 150;
const HTML_NS = "http://www.w3.org/1999/xhtml";

/**
 * Helper for long-running processes that should yield occasionally to
 * the mainloop.
 *
 * @param {Window} win
 *        Timeouts will be set on this window when appropriate.
 * @param {Array} array
 *        The array of items to process.
 * @param {Object} options
 *        Options for the update process:
 *          onItem {function} Will be called with the value of each iteration.
 *          onBatch {function} Will be called after each batch of iterations,
 *            before yielding to the main loop.
 *          onDone {function} Will be called when iteration is complete.
 *          onCancel {function} Will be called if the process is canceled.
 *          threshold {int} How long to process before yielding, in ms.
 */
function UpdateProcess(win, array, options) {
  this.win = win;
  this.index = 0;
  this.array = array;

  this.onItem = options.onItem || function() {};
  this.onBatch = options.onBatch || function() {};
  this.onDone = options.onDone || function() {};
  this.onCancel = options.onCancel || function() {};
  this.threshold = options.threshold || 45;

  this.canceled = false;
}

UpdateProcess.prototype = {
  /**
   * Error thrown when the array of items to process is empty.
   */
  ERROR_ITERATION_DONE: new Error("UpdateProcess iteration done"),

  /**
   * Schedule a new batch on the main loop.
   */
  schedule: function() {
    if (this.canceled) {
      return;
    }
    this._timeout = setTimeout(this._timeoutHandler.bind(this), 0);
  },

  /**
   * Cancel the running process.  onItem will not be called again,
   * and onCancel will be called.
   */
  cancel: function() {
    if (this._timeout) {
      clearTimeout(this._timeout);
      this._timeout = 0;
    }
    this.canceled = true;
    this.onCancel();
  },

  _timeoutHandler: function() {
    this._timeout = null;
    try {
      this._runBatch();
      this.schedule();
    } catch (e) {
      if (e === this.ERROR_ITERATION_DONE) {
        this.onBatch();
        this.onDone();
        return;
      }
      console.error(e);
      throw e;
    }
  },

  _runBatch: function() {
    const time = Date.now();
    while (!this.canceled) {
      const next = this._next();
      this.onItem(next);
      if ((Date.now() - time) > this.threshold) {
        this.onBatch();
        return;
      }
    }
  },

  /**
   * Returns the item at the current index and increases the index.
   * If all items have already been processed, will throw ERROR_ITERATION_DONE.
   */
  _next: function() {
    if (this.index < this.array.length) {
      return this.array[this.index++];
    }
    throw this.ERROR_ITERATION_DONE;
  },
};

/**
 * CssComputedView is a panel that manages the display of a table
 * sorted by style. There should be one instance of CssComputedView
 * per style display (of which there will generally only be one).
 *
 * @param {Inspector} inspector
 *        Inspector toolbox panel
 * @param {Document} document
 *        The document that will contain the computed view.
 * @param {PageStyleFront} pageStyle
 *        Front for the page style actor that will be providing
 *        the style information.
 */
function CssComputedView(inspector, document, pageStyle) {
  this.inspector = inspector;
  this.styleDocument = document;
  this.styleWindow = this.styleDocument.defaultView;
  this.pageStyle = pageStyle;

  this.propertyViews = [];

  const cssProperties = getCssProperties(inspector.toolbox);
  this._outputParser = new OutputParser(document, cssProperties);

  // Create bound methods.
  this.focusWindow = this.focusWindow.bind(this);
  this._onClearSearch = this._onClearSearch.bind(this);
  this._onClick = this._onClick.bind(this);
  this._onContextMenu = this._onContextMenu.bind(this);
  this._onCopy = this._onCopy.bind(this);
  this._onFilterStyles = this._onFilterStyles.bind(this);
  this._onIncludeBrowserStyles = this._onIncludeBrowserStyles.bind(this);

  const doc = this.styleDocument;
  this.element = doc.getElementById("computed-property-container");
  this.searchField = doc.getElementById("computed-searchbox");
  this.searchClearButton = doc.getElementById("computed-searchinput-clear");
  this.includeBrowserStylesCheckbox = doc.getElementById("browser-style-checkbox");

  this.shortcuts = new KeyShortcuts({ window: this.styleWindow });
  this._onShortcut = this._onShortcut.bind(this);
  this.shortcuts.on("CmdOrCtrl+F", event => this._onShortcut("CmdOrCtrl+F", event));
  this.shortcuts.on("Escape", event => this._onShortcut("Escape", event));
  this.styleDocument.addEventListener("copy", this._onCopy);
  this.styleDocument.addEventListener("mousedown", this.focusWindow);
  this.element.addEventListener("click", this._onClick);
  this.element.addEventListener("contextmenu", this._onContextMenu);
  this.element.addEventListener("mousemove", () => {
    this.addHighlightersToView();
  }, { once: true });
  this.searchField.addEventListener("input", this._onFilterStyles);
  this.searchClearButton.addEventListener("click", this._onClearSearch);
  this.includeBrowserStylesCheckbox.addEventListener("input",
    this._onIncludeBrowserStyles);

  this.searchClearButton.hidden = true;

  // No results text.
  this.noResults = this.styleDocument.getElementById("computed-no-results");

  // Refresh panel when color unit changed or pref for showing
  // original sources changes.
  this._handlePrefChange = this._handlePrefChange.bind(this);
  this._prefObserver = new PrefObserver("devtools.");
  this._prefObserver.on("devtools.defaultColorUnit", this._handlePrefChange);

  // The element that we're inspecting, and the document that it comes from.
  this._viewedElement = null;

  this.createStyleViews();

  // Add the tooltips and highlightersoverlay
  this.tooltips = new TooltipsOverlay(this);
}

/**
 * Lookup a l10n string in the shared styleinspector string bundle.
 *
 * @param {String} name
 *        The key to lookup.
 * @returns {String} localized version of the given key.
 */
CssComputedView.l10n = function(name) {
  try {
    return STYLE_INSPECTOR_L10N.getStr(name);
  } catch (ex) {
    console.log("Error reading '" + name + "'");
    throw new Error("l10n error with " + name);
  }
};

CssComputedView.prototype = {
  // Cache the list of properties that match the selected element.
  _matchedProperties: null,

  // Used for cancelling timeouts in the style filter.
  _filterChangedTimeout: null,

  // Holds the ID of the panelRefresh timeout.
  _panelRefreshTimeout: null,

  // Toggle for zebra striping
  _darkStripe: true,

  // Number of visible properties
  numVisibleProperties: 0,

  get contextMenu() {
    if (!this._contextMenu) {
      this._contextMenu = new StyleInspectorMenu(this, { isRuleView: false });
    }

    return this._contextMenu;
  },

  // Get the highlighters overlay from the Inspector.
  get highlighters() {
    if (!this._highlighters) {
      // highlighters is a lazy getter in the inspector.
      this._highlighters = this.inspector.highlighters;
    }

    return this._highlighters;
  },

  setPageStyle: function(pageStyle) {
    this.pageStyle = pageStyle;
  },

  get includeBrowserStyles() {
    return this.includeBrowserStylesCheckbox.checked;
  },

  _handlePrefChange: function(event, data) {
    if (this._computed) {
      this.refreshPanel();
    }
  },

  /**
   * Update the view with a new selected element. The CssComputedView panel
   * will show the style information for the given element.
   *
   * @param {NodeFront} element
   *        The highlighted node to get styles for.
   * @returns a promise that will be resolved when highlighting is complete.
   */
  selectElement: function(element) {
    if (!element) {
      this._viewedElement = null;
      this.noResults.hidden = false;

      if (this._refreshProcess) {
        this._refreshProcess.cancel();
      }
      // Hiding all properties
      for (const propView of this.propertyViews) {
        propView.refresh();
      }
      return promise.resolve(undefined);
    }

    if (element === this._viewedElement) {
      return promise.resolve(undefined);
    }

    this._viewedElement = element;
    this.refreshSourceFilter();

    return this.refreshPanel();
  },

  /**
   * Get the type of a given node in the computed-view
   *
   * @param {DOMNode} node
   *        The node which we want information about
   * @return {Object} The type information object contains the following props:
   * - view {String} Always "computed" to indicate the computed view.
   * - type {String} One of the VIEW_NODE_XXX_TYPE const in
   *   client/inspector/shared/node-types
   * - value {Object} Depends on the type of the node
   * returns null if the node isn't anything we care about
   */
  getNodeInfo: function(node) {
    if (!node) {
      return null;
    }

    const classes = node.classList;

    // Check if the node isn't a selector first since this doesn't require
    // walking the DOM
    if (classes.contains("matched") ||
        classes.contains("bestmatch") ||
        classes.contains("parentmatch")) {
      let selectorText = "";

      for (const child of node.childNodes[0].childNodes) {
        if (child.nodeType === node.TEXT_NODE) {
          selectorText += child.textContent;
        }
      }
      return {
        type: VIEW_NODE_SELECTOR_TYPE,
        value: selectorText.trim()
      };
    }

    // Walk up the nodes to find out where node is
    let propertyView;
    let propertyContent;
    let parent = node;
    while (parent.parentNode) {
      if (parent.classList.contains("computed-property-view")) {
        propertyView = parent;
        break;
      }
      if (parent.classList.contains("computed-property-content")) {
        propertyContent = parent;
        break;
      }
      parent = parent.parentNode;
    }
    if (!propertyView && !propertyContent) {
      return null;
    }

    let value, type;

    // Get the property and value for a node that's a property name or value
    const isHref = classes.contains("theme-link") && !classes.contains("computed-link");
    if (propertyView && (classes.contains("computed-property-name") ||
                         classes.contains("computed-property-value") ||
                         isHref)) {
      value = {
        property: parent.querySelector(".computed-property-name").firstChild.textContent,
        value: parent.querySelector(".computed-property-value").textContent
      };
    }
    if (propertyContent && (classes.contains("computed-other-property-value") ||
                            isHref)) {
      const view = propertyContent.previousSibling;
      value = {
        property: view.querySelector(".computed-property-name").firstChild.textContent,
        value: node.textContent
      };
    }
    if (classes.contains("computed-font-family")) {
      if (propertyView) {
        value = {
          property: parent.querySelector(
            ".computed-property-name").firstChild.textContent,
          value: node.parentNode.textContent
        };
      } else if (propertyContent) {
        const view = propertyContent.previousSibling;
        value = {
          property: view.querySelector(".computed-property-name").firstChild.textContent,
          value: node.parentNode.textContent
        };
      } else {
        return null;
      }
    }

    // Get the type
    if (classes.contains("computed-property-name")) {
      type = VIEW_NODE_PROPERTY_TYPE;
    } else if (classes.contains("computed-property-value") ||
               classes.contains("computed-other-property-value")) {
      type = VIEW_NODE_VALUE_TYPE;
    } else if (classes.contains("computed-font-family")) {
      type = VIEW_NODE_FONT_TYPE;
    } else if (isHref) {
      type = VIEW_NODE_IMAGE_URL_TYPE;
      value.url = node.href;
    } else {
      return null;
    }

    return {
      view: "computed",
      type,
      value,
    };
  },

  _createPropertyViews: function() {
    if (this._createViewsPromise) {
      return this._createViewsPromise;
    }

    this.refreshSourceFilter();
    this.numVisibleProperties = 0;
    const fragment = this.styleDocument.createDocumentFragment();

    this._createViewsPromise = new Promise((resolve, reject) => {
      this._createViewsProcess = new UpdateProcess(
        this.styleWindow, CssComputedView.propertyNames, {
          onItem: (propertyName) => {
            // Per-item callback.
            const propView = new PropertyView(this, propertyName);
            fragment.appendChild(propView.buildMain());
            fragment.appendChild(propView.buildSelectorContainer());

            if (propView.visible) {
              this.numVisibleProperties++;
            }
            this.propertyViews.push(propView);
          },
          onCancel: () => {
            reject("_createPropertyViews cancelled");
          },
          onDone: () => {
            // Completed callback.
            this.element.appendChild(fragment);
            this.noResults.hidden = this.numVisibleProperties > 0;
            resolve(undefined);
          }
        }
      );
    });

    this._createViewsProcess.schedule();

    return this._createViewsPromise;
  },

  /**
   * Refresh the panel content.
   */
  refreshPanel: function() {
    if (!this._viewedElement) {
      return promise.resolve();
    }

    // Capture the current viewed element to return from the promise handler
    // early if it changed
    const viewedElement = this._viewedElement;

    return promise.all([
      this._createPropertyViews(),
      this.pageStyle.getComputed(this._viewedElement, {
        filter: this._sourceFilter,
        onlyMatched: !this.includeBrowserStyles,
        markMatched: true
      })
    ]).then(([, computed]) => {
      if (viewedElement !== this._viewedElement) {
        return promise.resolve();
      }

      this._matchedProperties = new Set();
      for (const name in computed) {
        if (computed[name].matched) {
          this._matchedProperties.add(name);
        }
      }
      this._computed = computed;

      if (this._refreshProcess) {
        this._refreshProcess.cancel();
      }

      this.noResults.hidden = true;

      // Reset visible property count
      this.numVisibleProperties = 0;

      // Reset zebra striping.
      this._darkStripe = true;

      return new Promise((resolve, reject) => {
        this._refreshProcess = new UpdateProcess(
          this.styleWindow, this.propertyViews, {
            onItem: (propView) => {
              propView.refresh();
            },
            onCancel: () => {
              reject("_refreshProcess of computed view cancelled");
            },
            onDone: () => {
              this._refreshProcess = null;
              this.noResults.hidden = this.numVisibleProperties > 0;

              if (this.searchField.value.length > 0 &&
                  !this.numVisibleProperties) {
                this.searchField.classList
                                .add("devtools-style-searchbox-no-match");
              } else {
                this.searchField.classList
                                .remove("devtools-style-searchbox-no-match");
              }

              this.inspector.emit("computed-view-refreshed");
              resolve(undefined);
            }
          }
        );
        this._refreshProcess.schedule();
      });
    }).catch(console.error);
  },

  /**
   * Handle the shortcut events in the computed view.
   */
  _onShortcut: function(name, event) {
    if (!event.target.closest("#sidebar-panel-computedview")) {
      return;
    }
    // Handle the search box's keypress event. If the escape key is pressed,
    // clear the search box field.
    if (name === "Escape" && event.target === this.searchField &&
        this._onClearSearch()) {
      event.preventDefault();
      event.stopPropagation();
    } else if (name === "CmdOrCtrl+F") {
      this.searchField.focus();
      event.preventDefault();
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

    const filterTimeout = (this.searchField.value.length > 0)
      ? FILTER_CHANGED_TIMEOUT : 0;
    this.searchClearButton.hidden = this.searchField.value.length === 0;

    this._filterChangedTimeout = setTimeout(() => {
      if (this.searchField.value.length > 0) {
        this.searchField.setAttribute("filled", true);
      } else {
        this.searchField.removeAttribute("filled");
      }

      this.refreshPanel();
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

  /**
   * The change event handler for the includeBrowserStyles checkbox.
   */
  _onIncludeBrowserStyles: function() {
    this.refreshSourceFilter();
    this.refreshPanel();
  },

  /**
   * When includeBrowserStylesCheckbox.checked is false we only display
   * properties that have matched selectors and have been included by the
   * document or one of thedocument's stylesheets. If .checked is false we
   * display all properties including those that come from UA stylesheets.
   */
  refreshSourceFilter: function() {
    this._matchedProperties = null;
    this._sourceFilter = this.includeBrowserStyles ?
                                 CssLogic.FILTER.UA :
                                 CssLogic.FILTER.USER;
  },

  /**
   * The CSS as displayed by the UI.
   */
  createStyleViews: function() {
    if (CssComputedView.propertyNames) {
      return;
    }

    CssComputedView.propertyNames = [];

    // Here we build and cache a list of css properties supported by the browser
    // We could use any element but let's use the main document's root element
    const styles = this.styleWindow
      .getComputedStyle(this.styleDocument.documentElement);
    const mozProps = [];
    for (let i = 0, numStyles = styles.length; i < numStyles; i++) {
      const prop = styles.item(i);
      if (prop.startsWith("--")) {
        // Skip any CSS variables used inside of browser CSS files
        continue;
      } else if (prop.startsWith("-")) {
        mozProps.push(prop);
      } else {
        CssComputedView.propertyNames.push(prop);
      }
    }

    CssComputedView.propertyNames.sort();
    CssComputedView.propertyNames.push.apply(CssComputedView.propertyNames,
      mozProps.sort());

    this._createPropertyViews().catch(e => {
      if (!this._isDestroyed) {
        console.warn("The creation of property views was cancelled because " +
          "the computed-view was destroyed before it was done creating views");
      } else {
        console.error(e);
      }
    });
  },

  /**
   * Get a set of properties that have matched selectors.
   *
   * @return {Set} If a property name is in the set, it has matching selectors.
   */
  get matchedProperties() {
    return this._matchedProperties || new Set();
  },

  /**
   * Focus the window on mousedown.
   */
  focusWindow: function() {
    this.styleWindow.focus();
  },

  /**
   * Context menu handler.
   */
  _onContextMenu: function(event) {
    this.contextMenu.show(event);
  },

  _onClick: function(event) {
    const target = event.target;

    if (target.nodeName === "a") {
      event.stopPropagation();
      event.preventDefault();
      const browserWin = this.inspector.target.tab.ownerDocument.defaultView;
      browserWin.openWebLinkIn(target.href, "tab");
    }
  },

  /**
   * Callback for copy event. Copy selected text.
   *
   * @param {Event} event
   *        copy event object.
   */
  _onCopy: function(event) {
    const win = this.styleWindow;
    const text = win.getSelection().toString().trim();
    if (text !== "") {
      this.copySelection();
      event.preventDefault();
    }
  },

  /**
   * Copy the current selection to the clipboard
   */
  copySelection: function() {
    try {
      const win = this.styleWindow;
      const text = win.getSelection().toString().trim();

      clipboardHelper.copyString(text);
    } catch (e) {
      console.error(e);
    }
  },

  /**
   * Adds the highlighters overlay to the computed view. This is called by the "mousemove"
   * event handler and in shared-head.js when opening and selecting the computed view.
   */
  addHighlightersToView() {
    this.highlighters.addToView(this);
  },

  /**
   * Destructor for CssComputedView.
   */
  destroy: function() {
    this._viewedElement = null;
    this._outputParser = null;

    this._prefObserver.off("devtools.defaultColorUnit", this._handlePrefChange);
    this._prefObserver.destroy();

    // Cancel tree construction
    if (this._createViewsProcess) {
      this._createViewsProcess.cancel();
    }
    if (this._refreshProcess) {
      this._refreshProcess.cancel();
    }

    if (this._contextMenu) {
      this._contextMenu.destroy();
      this._contextMenu = null;
    }

    if (this._highlighters) {
      this._highlighters.removeFromView(this);
      this._highlighters = null;
    }

    this.tooltips.destroy();

    // Remove bound listeners
    this.element.removeEventListener("click", this._onClick);
    this.element.removeEventListener("contextmenu", this._onContextMenu);
    this.searchField.removeEventListener("input", this._onFilterStyles);
    this.searchClearButton.removeEventListener("click", this._onClearSearch);
    this.styleDocument.removeEventListener("copy", this._onCopy);
    this.styleDocument.removeEventListener("mousedown", this.focusWindow);
    this.includeBrowserStylesCheckbox.removeEventListener("input",
      this._onIncludeBrowserStyles);

    // Nodes used in templating
    this.element = null;
    this.searchField = null;
    this.searchClearButton = null;
    this.includeBrowserStylesCheckbox = null;

    // Property views
    for (const propView of this.propertyViews) {
      propView.destroy();
    }
    this.propertyViews = null;

    this.inspector = null;
    this.styleDocument = null;
    this.styleWindow = null;

    this._isDestroyed = true;
  }
};

function PropertyInfo(tree, name) {
  this.tree = tree;
  this.name = name;
}

PropertyInfo.prototype = {
  get value() {
    if (this.tree._computed) {
      const value = this.tree._computed[this.name].value;
      return value;
    }
    return null;
  }
};

/**
 * A container to give easy access to property data from the template engine.
 *
 * @param {CssComputedView} tree
 *        The CssComputedView instance we are working with.
 * @param {String} name
 *        The CSS property name for which this PropertyView
 *        instance will render the rules.
 */
function PropertyView(tree, name) {
  this.tree = tree;
  this.name = name;

  this.link = "https://developer.mozilla.org/CSS/" + name;

  this._propertyInfo = new PropertyInfo(tree, name);
}

PropertyView.prototype = {
  // The parent element which contains the open attribute
  element: null,

  // Property header node
  propertyHeader: null,

  // Destination for property names
  nameNode: null,

  // Destination for property values
  valueNode: null,

  // Are matched rules expanded?
  matchedExpanded: false,

  // Matched selector container
  matchedSelectorsContainer: null,

  // Matched selector expando
  matchedExpander: null,

  // Cache for matched selector views
  _matchedSelectorViews: null,

  // The previously selected element used for the selector view caches
  _prevViewedElement: null,

  /**
   * Get the computed style for the current property.
   *
   * @return {String} the computed style for the current property of the
   * currently highlighted element.
   */
  get value() {
    return this.propertyInfo.value;
  },

  /**
   * An easy way to access the CssPropertyInfo behind this PropertyView.
   */
  get propertyInfo() {
    return this._propertyInfo;
  },

  /**
   * Does the property have any matched selectors?
   */
  get hasMatchedSelectors() {
    return this.tree.matchedProperties.has(this.name);
  },

  /**
   * Should this property be visible?
   */
  get visible() {
    if (!this.tree._viewedElement) {
      return false;
    }

    if (!this.tree.includeBrowserStyles && !this.hasMatchedSelectors) {
      return false;
    }

    const searchTerm = this.tree.searchField.value.toLowerCase();
    const isValidSearchTerm = searchTerm.trim().length > 0;
    if (isValidSearchTerm &&
        !this.name.toLowerCase().includes(searchTerm) &&
        !this.value.toLowerCase().includes(searchTerm)) {
      return false;
    }

    return true;
  },

  /**
   * Returns the className that should be assigned to the propertyView.
   *
   * @return {String}
   */
  get propertyHeaderClassName() {
    if (this.visible) {
      const isDark = this.tree._darkStripe = !this.tree._darkStripe;
      return isDark ? "computed-property-view row-striped" : "computed-property-view";
    }
    return "computed-property-hidden";
  },

  /**
   * Returns the className that should be assigned to the propertyView content
   * container.
   *
   * @return {String}
   */
  get propertyContentClassName() {
    if (this.visible) {
      const isDark = this.tree._darkStripe;
      return isDark
        ? "computed-property-content row-striped"
        : "computed-property-content";
    }
    return "computed-property-hidden";
  },

  /**
   * Build the markup for on computed style
   *
   * @return {Element}
   */
  buildMain: function() {
    const doc = this.tree.styleDocument;

    // Build the container element
    this.onMatchedToggle = this.onMatchedToggle.bind(this);
    this.element = doc.createElementNS(HTML_NS, "div");
    this.element.setAttribute("class", this.propertyHeaderClassName);
    this.element.addEventListener("dblclick", this.onMatchedToggle);

    // Make it keyboard navigable
    this.element.setAttribute("tabindex", "0");
    this.shortcuts = new KeyShortcuts({
      window: this.tree.styleWindow,
      target: this.element
    });
    this.shortcuts.on("F1", event => {
      this.mdnLinkClick(event);
      // Prevent opening the options panel
      event.preventDefault();
      event.stopPropagation();
    });
    this.shortcuts.on("Return", this.onMatchedToggle);
    this.shortcuts.on("Space", this.onMatchedToggle);

    const nameContainer = doc.createElementNS(HTML_NS, "span");
    nameContainer.className = "computed-property-name-container";
    this.element.appendChild(nameContainer);

    // Build the twisty expand/collapse
    this.matchedExpander = doc.createElementNS(HTML_NS, "div");
    this.matchedExpander.className = "computed-expander theme-twisty";
    this.matchedExpander.addEventListener("click", this.onMatchedToggle);
    nameContainer.appendChild(this.matchedExpander);

    // Build the style name element
    this.nameNode = doc.createElementNS(HTML_NS, "span");
    this.nameNode.classList.add("computed-property-name", "theme-fg-color5");
    // Reset its tabindex attribute otherwise, if an ellipsis is applied
    // it will be reachable via TABing
    this.nameNode.setAttribute("tabindex", "");
    // Avoid english text (css properties) from being altered
    // by RTL mode
    this.nameNode.setAttribute("dir", "ltr");
    this.nameNode.textContent = this.nameNode.title = this.name;
    // Make it hand over the focus to the container
    this.onFocus = () => this.element.focus();
    this.nameNode.addEventListener("click", this.onFocus);

    // Build the style name ":" separator
    const nameSeparator = doc.createElementNS(HTML_NS, "span");
    nameSeparator.classList.add("visually-hidden");
    nameSeparator.textContent = ": ";
    this.nameNode.appendChild(nameSeparator);

    nameContainer.appendChild(this.nameNode);

    const valueContainer = doc.createElementNS(HTML_NS, "span");
    valueContainer.className = "computed-property-value-container";
    this.element.appendChild(valueContainer);

    // Build the style value element
    this.valueNode = doc.createElementNS(HTML_NS, "span");
    this.valueNode.classList.add("computed-property-value", "theme-fg-color1");
    // Reset its tabindex attribute otherwise, if an ellipsis is applied
    // it will be reachable via TABing
    this.valueNode.setAttribute("tabindex", "");
    this.valueNode.setAttribute("dir", "ltr");
    // Make it hand over the focus to the container
    this.valueNode.addEventListener("click", this.onFocus);

    // Build the style value ";" separator
    const valueSeparator = doc.createElementNS(HTML_NS, "span");
    valueSeparator.classList.add("visually-hidden");
    valueSeparator.textContent = ";";

    valueContainer.appendChild(this.valueNode);
    valueContainer.appendChild(valueSeparator);

    return this.element;
  },

  buildSelectorContainer: function() {
    const doc = this.tree.styleDocument;
    const element = doc.createElementNS(HTML_NS, "div");
    element.setAttribute("class", this.propertyContentClassName);
    this.matchedSelectorsContainer = doc.createElementNS(HTML_NS, "div");
    this.matchedSelectorsContainer.classList.add("matchedselectors");
    element.appendChild(this.matchedSelectorsContainer);

    return element;
  },

  /**
   * Refresh the panel's CSS property value.
   */
  refresh: function() {
    this.element.className = this.propertyHeaderClassName;
    this.element.nextElementSibling.className = this.propertyContentClassName;

    if (this._prevViewedElement !== this.tree._viewedElement) {
      this._matchedSelectorViews = null;
      this._prevViewedElement = this.tree._viewedElement;
    }

    if (!this.tree._viewedElement || !this.visible) {
      this.valueNode.textContent = this.valueNode.title = "";
      this.matchedSelectorsContainer.parentNode.hidden = true;
      this.matchedSelectorsContainer.textContent = "";
      this.matchedExpander.removeAttribute("open");
      return;
    }

    this.tree.numVisibleProperties++;

    const outputParser = this.tree._outputParser;
    const frag = outputParser.parseCssProperty(this.propertyInfo.name,
      this.propertyInfo.value,
      {
        colorSwatchClass: "computed-colorswatch",
        colorClass: "computed-color",
        urlClass: "theme-link",
        fontFamilyClass: "computed-font-family",
        // No need to use baseURI here as computed URIs are never relative.
      });
    this.valueNode.innerHTML = "";
    this.valueNode.appendChild(frag);

    this.refreshMatchedSelectors();
  },

  /**
   * Refresh the panel matched rules.
   */
  refreshMatchedSelectors: function() {
    const hasMatchedSelectors = this.hasMatchedSelectors;
    this.matchedSelectorsContainer.parentNode.hidden = !hasMatchedSelectors;

    if (hasMatchedSelectors) {
      this.matchedExpander.classList.add("computed-expandable");
    } else {
      this.matchedExpander.classList.remove("computed-expandable");
    }

    if (this.matchedExpanded && hasMatchedSelectors) {
      return this.tree.pageStyle
        .getMatchedSelectors(this.tree._viewedElement, this.name)
        .then(matched => {
          if (!this.matchedExpanded) {
            return;
          }

          this._matchedSelectorResponse = matched;

          this._buildMatchedSelectors();
          this.matchedExpander.setAttribute("open", "");
          this.tree.inspector.emit("computed-view-property-expanded");
        }).catch(console.error);
    }

    this.matchedSelectorsContainer.innerHTML = "";
    this.matchedExpander.removeAttribute("open");
    this.tree.inspector.emit("computed-view-property-collapsed");
    return promise.resolve(undefined);
  },

  get matchedSelectors() {
    return this._matchedSelectorResponse;
  },

  _buildMatchedSelectors: function() {
    const frag = this.element.ownerDocument.createDocumentFragment();

    for (const selector of this.matchedSelectorViews) {
      const p = createChild(frag, "p");
      const span = createChild(p, "span", {
        class: "rule-link"
      });
      const link = createChild(span, "a", {
        target: "_blank",
        class: "computed-link theme-link",
        title: selector.href,
        sourcelocation: selector.source,
        tabindex: "0",
        textContent: selector.source
      });
      link.addEventListener("click", selector.openStyleEditor);
      const shortcuts = new KeyShortcuts({
        window: this.tree.styleWindow,
        target: link
      });
      shortcuts.on("Return", () => selector.openStyleEditor());

      const status = createChild(p, "span", {
        dir: "ltr",
        class: "rule-text theme-fg-color3 " + selector.statusClass,
        title: selector.statusText
      });

      createChild(status, "div", {
        class: "fix-get-selection",
        textContent: selector.sourceText
      });

      const valueDiv = createChild(status, "div", {
        class: "fix-get-selection computed-other-property-value theme-fg-color1"
      });
      valueDiv.appendChild(selector.outputFragment);
    }

    this.matchedSelectorsContainer.innerHTML = "";
    this.matchedSelectorsContainer.appendChild(frag);
  },

  /**
   * Provide access to the matched SelectorViews that we are currently
   * displaying.
   */
  get matchedSelectorViews() {
    if (!this._matchedSelectorViews) {
      this._matchedSelectorViews = [];
      this._matchedSelectorResponse.forEach(selectorInfo => {
        const selectorView = new SelectorView(this.tree, selectorInfo);
        this._matchedSelectorViews.push(selectorView);
      }, this);
    }
    return this._matchedSelectorViews;
  },

  /**
   * The action when a user expands matched selectors.
   *
   * @param {Event} event
   *        Used to determine the class name of the targets click
   *        event.
   */
  onMatchedToggle: function(event) {
    if (event.shiftKey) {
      return;
    }
    this.matchedExpanded = !this.matchedExpanded;
    this.refreshMatchedSelectors();
    event.preventDefault();
  },

  /**
   * The action when a user clicks on the MDN help link for a property.
   */
  mdnLinkClick: function(event) {
    const inspector = this.tree.inspector;

    if (inspector.target.tab) {
      const browserWin = inspector.target.tab.ownerDocument.defaultView;
      browserWin.openWebLinkIn(this.link, "tab");
    }
  },

  /**
   * Destroy this property view, removing event listeners
   */
  destroy: function() {
    if (this._matchedSelectorViews) {
      for (const view of this._matchedSelectorViews) {
        view.destroy();
      }
    }

    this.element.removeEventListener("dblclick", this.onMatchedToggle);
    this.shortcuts.destroy();
    this.element = null;

    this.matchedExpander.removeEventListener("click", this.onMatchedToggle);
    this.matchedExpander = null;

    this.nameNode.removeEventListener("click", this.onFocus);
    this.nameNode = null;

    this.valueNode.removeEventListener("click", this.onFocus);
    this.valueNode = null;
  }
};

/**
 * A container to give us easy access to display data from a CssRule
 *
 * @param CssComputedView tree
 *        the owning CssComputedView
 * @param selectorInfo
 */
function SelectorView(tree, selectorInfo) {
  this.tree = tree;
  this.selectorInfo = selectorInfo;
  this._cacheStatusNames();

  this.openStyleEditor = this.openStyleEditor.bind(this);
  this._updateLocation = this._updateLocation.bind(this);

  const rule = this.selectorInfo.rule;
  if (!rule || !rule.parentStyleSheet || rule.type == ELEMENT_STYLE) {
    this.source = CssLogic.l10n("rule.sourceElement");
  } else {
    // This always refers to the generated location.
    const sheet = rule.parentStyleSheet;
    this.source = CssLogic.shortSource(sheet) + ":" + rule.line;

    const url = sheet.href || sheet.nodeHref;
    this.currentLocation = {
      href: url,
      line: rule.line,
      column: rule.column,
    };
    this.generatedLocation = this.currentLocation;
    this.sourceMapURLService = this.tree.inspector.toolbox.sourceMapURLService;
    this.sourceMapURLService.subscribe(url, rule.line, rule.column, this._updateLocation);
  }
}

/**
 * Decode for cssInfo.rule.status
 * @see SelectorView.prototype._cacheStatusNames
 * @see CssLogic.STATUS
 */
SelectorView.STATUS_NAMES = [
  // "Parent Match", "Matched", "Best Match"
];

SelectorView.CLASS_NAMES = [
  "parentmatch", "matched", "bestmatch"
];

SelectorView.prototype = {
  /**
   * Cache localized status names.
   *
   * These statuses are localized inside the styleinspector.properties string
   * bundle.
   * @see css-logic.js - the CssLogic.STATUS array.
   */
  _cacheStatusNames: function() {
    if (SelectorView.STATUS_NAMES.length) {
      return;
    }

    for (const status in CssLogic.STATUS) {
      const i = CssLogic.STATUS[status];
      if (i > CssLogic.STATUS.UNMATCHED) {
        const value = CssComputedView.l10n("rule.status." + status);
        // Replace normal spaces with non-breaking spaces
        SelectorView.STATUS_NAMES[i] = value.replace(/ /g, "\u00A0");
      }
    }
  },

  /**
   * A localized version of cssRule.status
   */
  get statusText() {
    return SelectorView.STATUS_NAMES[this.selectorInfo.status];
  },

  /**
   * Get class name for selector depending on status
   */
  get statusClass() {
    return SelectorView.CLASS_NAMES[this.selectorInfo.status - 1];
  },

  get href() {
    if (this._href) {
      return this._href;
    }
    const sheet = this.selectorInfo.rule.parentStyleSheet;
    this._href = sheet ? sheet.href : "#";
    return this._href;
  },

  get sourceText() {
    return this.selectorInfo.sourceText;
  },

  get value() {
    return this.selectorInfo.value;
  },

  get outputFragment() {
    // Sadly, because this fragment is added to the template by DOM Templater
    // we lose any events that are attached. This means that URLs will open in a
    // new window. At some point we should fix this by stopping using the
    // templater.
    const outputParser = this.tree._outputParser;
    const frag = outputParser.parseCssProperty(
      this.selectorInfo.name,
      this.selectorInfo.value, {
        colorSwatchClass: "computed-colorswatch",
        colorClass: "computed-color",
        urlClass: "theme-link",
        fontFamilyClass: "computed-font-family",
        baseURI: this.selectorInfo.rule.href
      }
    );
    return frag;
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
    if (!this.tree.element) {
      return;
    }

    // Update |currentLocation| to be whichever location is being
    // displayed at the moment.
    if (enabled) {
      this.currentLocation = { href: url, line, column };
    } else {
      this.currentLocation = this.generatedLocation;
    }

    const selector = '[sourcelocation="' + this.source + '"]';
    const link = this.tree.element.querySelector(selector);
    if (link) {
      const text = CssLogic.shortSource(this.currentLocation) +
          ":" + this.currentLocation.line;
      link.textContent = text;
    }

    this.tree.inspector.emit("computed-view-sourcelinks-updated");
  },

  /**
   * When a css link is clicked this method is called in order to either:
   *   1. Open the link in view source (for chrome stylesheets).
   *   2. Open the link in the style editor.
   *
   *   We can only view stylesheets contained in document.styleSheets inside the
   *   style editor.
   */
  openStyleEditor: function() {
    const inspector = this.tree.inspector;
    const rule = this.selectorInfo.rule;

    // The style editor can only display stylesheets coming from content because
    // chrome stylesheets are not listed in the editor's stylesheet selector.
    //
    // If the stylesheet is a content stylesheet we send it to the style
    // editor else we display it in the view source window.
    const parentStyleSheet = rule.parentStyleSheet;
    if (!parentStyleSheet || parentStyleSheet.isSystem) {
      const toolbox = gDevTools.getToolbox(inspector.target);
      toolbox.viewSource(rule.href, rule.line);
      return;
    }

    const {href, line, column} = this.currentLocation;
    const target = inspector.target;
    if (ToolDefinitions.styleEditor.isTargetSupported(target)) {
      gDevTools.showToolbox(target, "styleeditor").then(function(toolbox) {
        toolbox.getCurrentPanel().selectStyleSheet(href, line, column);
      });
    }
  },

  /**
   * Destroy this selector view, removing event listeners
   */
  destroy: function() {
    const rule = this.selectorInfo.rule;
    if (rule && rule.parentStyleSheet && rule.type != ELEMENT_STYLE) {
      const url = rule.parentStyleSheet.href || rule.parentStyleSheet.nodeHref;
      this.sourceMapURLService.unsubscribe(url, rule.line,
                                           rule.column, this._updateLocation);
    }
  },
};

function ComputedViewTool(inspector, window) {
  this.inspector = inspector;
  this.document = window.document;

  this.computedView = new CssComputedView(this.inspector, this.document,
    this.inspector.pageStyle);

  this.onSelected = this.onSelected.bind(this);
  this.refresh = this.refresh.bind(this);
  this.onPanelSelected = this.onPanelSelected.bind(this);

  this.inspector.selection.on("detached-front", this.onDetachedFront);
  this.inspector.selection.on("new-node-front", this.onSelected);
  this.inspector.selection.on("pseudoclass", this.refresh);
  this.inspector.sidebar.on("computedview-selected", this.onPanelSelected);
  this.inspector.pageStyle.on("stylesheet-updated", this.refresh);
  this.inspector.styleChangeTracker.on("style-changed", this.refresh);

  this.computedView.selectElement(null);

  this.onSelected();
}

ComputedViewTool.prototype = {
  isSidebarActive: function() {
    if (!this.computedView) {
      return false;
    }
    return this.inspector.sidebar.getCurrentTabID() == "computedview";
  },

  onDetachedFront: function() {
    this.onSelected(false);
  },

  onSelected: function(selectElement = true) {
    // Ignore the event if the view has been destroyed, or if it's inactive.
    // But only if the current selection isn't null. If it's been set to null,
    // let the update go through as this is needed to empty the view on
    // navigation.
    if (!this.computedView) {
      return;
    }

    const isInactive = !this.isSidebarActive() &&
                     this.inspector.selection.nodeFront;
    if (isInactive) {
      return;
    }

    this.computedView.setPageStyle(this.inspector.pageStyle);

    if (!this.inspector.selection.isConnected() ||
        !this.inspector.selection.isElementNode()) {
      this.computedView.selectElement(null);
      return;
    }

    if (selectElement) {
      const done = this.inspector.updating("computed-view");
      this.computedView.selectElement(this.inspector.selection.nodeFront).then(() => {
        done();
      });
    }
  },

  refresh: function() {
    if (this.isSidebarActive()) {
      this.computedView.refreshPanel();
    }
  },

  onPanelSelected: function() {
    if (this.inspector.selection.nodeFront === this.computedView._viewedElement) {
      this.refresh();
    } else {
      this.onSelected();
    }
  },

  destroy: function() {
    this.inspector.styleChangeTracker.off("style-changed", this.refresh);
    this.inspector.sidebar.off("computedview-selected", this.refresh);
    this.inspector.selection.off("pseudoclass", this.refresh);
    this.inspector.selection.off("new-node-front", this.onSelected);
    this.inspector.selection.off("detached-front", this.onDetachedFront);
    this.inspector.sidebar.off("computedview-selected", this.onPanelSelected);
    if (this.inspector.pageStyle) {
      this.inspector.pageStyle.off("stylesheet-updated", this.refresh);
    }

    this.computedView.destroy();

    this.computedView = this.document = this.inspector = null;
  }
};

exports.CssComputedView = CssComputedView;
exports.ComputedViewTool = ComputedViewTool;
exports.PropertyView = PropertyView;
