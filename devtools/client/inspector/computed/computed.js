/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* globals StopIteration */

"use strict";

/* eslint-disable mozilla/reject-some-requires */
const {Cc, Ci} = require("chrome");
/* eslint-enable mozilla/reject-some-requires */

const ToolDefinitions = require("devtools/client/definitions").Tools;
const CssLogic = require("devtools/shared/inspector/css-logic");
const {ELEMENT_STYLE} = require("devtools/shared/specs/styles");
const promise = require("promise");
const defer = require("devtools/shared/defer");
const Services = require("Services");
const {OutputParser} = require("devtools/client/shared/output-parser");
const {PrefObserver, PREF_ORIG_SOURCES} = require("devtools/client/styleeditor/utils");
const {createChild} = require("devtools/client/inspector/shared/utils");
const {gDevTools} = require("devtools/client/framework/devtools");
/* eslint-disable mozilla/reject-some-requires */
const {XPCOMUtils} = require("resource://gre/modules/XPCOMUtils.jsm");
/* eslint-enable mozilla/reject-some-requires */
const {getCssProperties} = require("devtools/shared/fronts/css-properties");

loader.lazyRequireGetter(this, "overlays",
  "devtools/client/inspector/shared/style-inspector-overlays");
loader.lazyRequireGetter(this, "StyleInspectorMenu",
  "devtools/client/inspector/shared/style-inspector-menu");
loader.lazyRequireGetter(this, "KeyShortcuts",
  "devtools/client/shared/key-shortcuts", true);
loader.lazyRequireGetter(this, "LayoutView",
  "devtools/client/inspector/layout/layout", true);

XPCOMUtils.defineLazyModuleGetter(this, "PluralForm",
                                  "resource://gre/modules/PluralForm.jsm");

XPCOMUtils.defineLazyGetter(CssComputedView, "_strings", function () {
  return Services.strings.createBundle(
    "chrome://devtools-shared/locale/styleinspector.properties");
});

XPCOMUtils.defineLazyGetter(this, "clipboardHelper", function () {
  return Cc["@mozilla.org/widget/clipboardhelper;1"]
         .getService(Ci.nsIClipboardHelper);
});

const FILTER_CHANGED_TIMEOUT = 150;
const HTML_NS = "http://www.w3.org/1999/xhtml";

/**
 * Helper for long-running processes that should yield occasionally to
 * the mainloop.
 *
 * @param {Window} win
 *        Timeouts will be set on this window when appropriate.
 * @param {Generator} generator
 *        Will iterate this generator.
 * @param {Object} options
 *        Options for the update process:
 *          onItem {function} Will be called with the value of each iteration.
 *          onBatch {function} Will be called after each batch of iterations,
 *            before yielding to the main loop.
 *          onDone {function} Will be called when iteration is complete.
 *          onCancel {function} Will be called if the process is canceled.
 *          threshold {int} How long to process before yielding, in ms.
 */
function UpdateProcess(win, generator, options) {
  this.win = win;
  this.iter = _Iterator(generator);
  this.onItem = options.onItem || function () {};
  this.onBatch = options.onBatch || function () {};
  this.onDone = options.onDone || function () {};
  this.onCancel = options.onCancel || function () {};
  this.threshold = options.threshold || 45;

  this.canceled = false;
}

UpdateProcess.prototype = {
  /**
   * Schedule a new batch on the main loop.
   */
  schedule: function () {
    if (this.canceled) {
      return;
    }
    this._timeout = setTimeout(this._timeoutHandler.bind(this), 0);
  },

  /**
   * Cancel the running process.  onItem will not be called again,
   * and onCancel will be called.
   */
  cancel: function () {
    if (this._timeout) {
      clearTimeout(this._timeout);
      this._timeout = 0;
    }
    this.canceled = true;
    this.onCancel();
  },

  _timeoutHandler: function () {
    this._timeout = null;
    try {
      this._runBatch();
      this.schedule();
    } catch (e) {
      if (e instanceof StopIteration) {
        this.onBatch();
        this.onDone();
        return;
      }
      console.error(e);
      throw e;
    }
  },

  _runBatch: function () {
    let time = Date.now();
    while (!this.canceled) {
      // Continue until iter.next() throws...
      let next = this.iter.next();
      this.onItem(next[1]);
      if ((Date.now() - time) > this.threshold) {
        this.onBatch();
        return;
      }
    }
  }
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

  let cssProperties = getCssProperties(inspector.toolbox);
  this._outputParser = new OutputParser(document, cssProperties.supportsType);

  let chromeReg = Cc["@mozilla.org/chrome/chrome-registry;1"]
    .getService(Ci.nsIXULChromeRegistry);
  this.getRTLAttr = chromeReg.isLocaleRTL("global") ? "rtl" : "ltr";

  // Create bound methods.
  this.focusWindow = this.focusWindow.bind(this);
  this._onContextMenu = this._onContextMenu.bind(this);
  this._onClick = this._onClick.bind(this);
  this._onCopy = this._onCopy.bind(this);
  this._onFilterStyles = this._onFilterStyles.bind(this);
  this._onClearSearch = this._onClearSearch.bind(this);
  this._onIncludeBrowserStyles = this._onIncludeBrowserStyles.bind(this);
  this._onFilterTextboxContextMenu =
    this._onFilterTextboxContextMenu.bind(this);

  let doc = this.styleDocument;
  this.element = doc.getElementById("propertyContainer");
  this.searchField = doc.getElementById("computedview-searchbox");
  this.searchClearButton = doc.getElementById("computedview-searchinput-clear");
  this.includeBrowserStylesCheckbox =
    doc.getElementById("browser-style-checkbox");

  this.shortcuts = new KeyShortcuts({ window: this.styleWindow });
  this._onShortcut = this._onShortcut.bind(this);
  this.shortcuts.on("CmdOrCtrl+F", this._onShortcut);
  this.shortcuts.on("Escape", this._onShortcut);
  this.styleDocument.addEventListener("mousedown", this.focusWindow);
  this.element.addEventListener("click", this._onClick);
  this.element.addEventListener("copy", this._onCopy);
  this.element.addEventListener("contextmenu", this._onContextMenu);
  this.searchField.addEventListener("input", this._onFilterStyles);
  this.searchField.addEventListener("contextmenu",
                                    this._onFilterTextboxContextMenu);
  this.searchClearButton.addEventListener("click", this._onClearSearch);
  this.includeBrowserStylesCheckbox.addEventListener("input",
    this._onIncludeBrowserStyles);

  this.searchClearButton.hidden = true;

  // No results text.
  this.noResults = this.styleDocument.getElementById("computedview-no-results");

  // Refresh panel when color unit changed.
  this._handlePrefChange = this._handlePrefChange.bind(this);
  gDevTools.on("pref-changed", this._handlePrefChange);

  // Refresh panel when pref for showing original sources changes
  this._onSourcePrefChanged = this._onSourcePrefChanged.bind(this);
  this._prefObserver = new PrefObserver("devtools.");
  this._prefObserver.on(PREF_ORIG_SOURCES, this._onSourcePrefChanged);

  // The element that we're inspecting, and the document that it comes from.
  this._viewedElement = null;

  this.createStyleViews();

  this._contextmenu = new StyleInspectorMenu(this, { isRuleView: false });

  // Add the tooltips and highlightersoverlay
  this.tooltips = new overlays.TooltipsOverlay(this);
  this.tooltips.addToView();

  this.highlighters = new overlays.HighlightersOverlay(this);
  this.highlighters.addToView();
}

/**
 * Memoized lookup of a l10n string from a string bundle.
 *
 * @param {String} name
 *        The key to lookup.
 * @returns {String} localized version of the given key.
 */
CssComputedView.l10n = function (name) {
  try {
    return CssComputedView._strings.GetStringFromName(name);
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

  setPageStyle: function (pageStyle) {
    this.pageStyle = pageStyle;
  },

  get includeBrowserStyles() {
    return this.includeBrowserStylesCheckbox.checked;
  },

  _handlePrefChange: function (event, data) {
    if (this._computed && (data.pref === "devtools.defaultColorUnit" ||
        data.pref === PREF_ORIG_SOURCES)) {
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
  selectElement: function (element) {
    if (!element) {
      this._viewedElement = null;
      this.noResults.hidden = false;

      if (this._refreshProcess) {
        this._refreshProcess.cancel();
      }
      // Hiding all properties
      for (let propView of this.propertyViews) {
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
   * - type {String} One of the VIEW_NODE_XXX_TYPE const in
   *   style-inspector-overlays
   * - value {Object} Depends on the type of the node
   * returns null if the node isn't anything we care about
   */
  getNodeInfo: function (node) {
    if (!node) {
      return null;
    }

    let classes = node.classList;

    // Check if the node isn't a selector first since this doesn't require
    // walking the DOM
    if (classes.contains("matched") ||
        classes.contains("bestmatch") ||
        classes.contains("parentmatch")) {
      let selectorText = "";
      for (let child of node.childNodes) {
        if (child.nodeType === node.TEXT_NODE) {
          selectorText += child.textContent;
        }
      }
      return {
        type: overlays.VIEW_NODE_SELECTOR_TYPE,
        value: selectorText.trim()
      };
    }

    // Walk up the nodes to find out where node is
    let propertyView;
    let propertyContent;
    let parent = node;
    while (parent.parentNode) {
      if (parent.classList.contains("property-view")) {
        propertyView = parent;
        break;
      }
      if (parent.classList.contains("property-content")) {
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
    let isHref = classes.contains("theme-link") && !classes.contains("link");
    if (propertyView && (classes.contains("property-name") ||
                         classes.contains("property-value") ||
                         isHref)) {
      value = {
        property: parent.querySelector(".property-name").textContent,
        value: parent.querySelector(".property-value").textContent
      };
    }
    if (propertyContent && (classes.contains("other-property-value") ||
                            isHref)) {
      let view = propertyContent.previousSibling;
      value = {
        property: view.querySelector(".property-name").textContent,
        value: node.textContent
      };
    }

    // Get the type
    if (classes.contains("property-name")) {
      type = overlays.VIEW_NODE_PROPERTY_TYPE;
    } else if (classes.contains("property-value") ||
               classes.contains("other-property-value")) {
      type = overlays.VIEW_NODE_VALUE_TYPE;
    } else if (isHref) {
      type = overlays.VIEW_NODE_IMAGE_URL_TYPE;
      value.url = node.href;
    } else {
      return null;
    }

    return {type, value};
  },

  _createPropertyViews: function () {
    if (this._createViewsPromise) {
      return this._createViewsPromise;
    }

    let deferred = defer();
    this._createViewsPromise = deferred.promise;

    this.refreshSourceFilter();
    this.numVisibleProperties = 0;
    let fragment = this.styleDocument.createDocumentFragment();

    this._createViewsProcess = new UpdateProcess(
      this.styleWindow, CssComputedView.propertyNames, {
        onItem: (propertyName) => {
          // Per-item callback.
          let propView = new PropertyView(this, propertyName);
          fragment.appendChild(propView.buildMain());
          fragment.appendChild(propView.buildSelectorContainer());

          if (propView.visible) {
            this.numVisibleProperties++;
          }
          this.propertyViews.push(propView);
        },
        onCancel: () => {
          deferred.reject("_createPropertyViews cancelled");
        },
        onDone: () => {
          // Completed callback.
          this.element.appendChild(fragment);
          this.noResults.hidden = this.numVisibleProperties > 0;
          deferred.resolve(undefined);
        }
      }
    );

    this._createViewsProcess.schedule();
    return deferred.promise;
  },

  /**
   * Refresh the panel content.
   */
  refreshPanel: function () {
    if (!this._viewedElement) {
      return promise.resolve();
    }

    // Capture the current viewed element to return from the promise handler
    // early if it changed
    let viewedElement = this._viewedElement;

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
      for (let name in computed) {
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

      let deferred = defer();
      this._refreshProcess = new UpdateProcess(
        this.styleWindow, this.propertyViews, {
          onItem: (propView) => {
            propView.refresh();
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
            deferred.resolve(undefined);
          }
        }
      );
      this._refreshProcess.schedule();
      return deferred.promise;
    }).then(null, (err) => console.error(err));
  },

  /**
   * Handle the shortcut events in the computed view.
   */
  _onShortcut: function (name, event) {
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

    let filterTimeout = (this.searchField.value.length > 0)
      ? FILTER_CHANGED_TIMEOUT : 0;
    this.searchClearButton.hidden = this.searchField.value.length === 0;

    this._filterChangedTimeout = setTimeout(() => {
      if (this.searchField.value.length > 0) {
        this.searchField.setAttribute("filled", true);
        this.inspector.emit("computed-view-filtered", true);
      } else {
        this.searchField.removeAttribute("filled");
        this.inspector.emit("computed-view-filtered", false);
      }

      this.refreshPanel();
      this._filterChangeTimeout = null;
    }, filterTimeout);
  },

  /**
   * Context menu handler for filter style search box.
   */
  _onFilterTextboxContextMenu: function (event) {
    try {
      this.styleDocument.defaultView.focus();
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

  /**
   * The change event handler for the includeBrowserStyles checkbox.
   */
  _onIncludeBrowserStyles: function () {
    this.refreshSourceFilter();
    this.refreshPanel();
  },

  /**
   * When includeBrowserStylesCheckbox.checked is false we only display
   * properties that have matched selectors and have been included by the
   * document or one of thedocument's stylesheets. If .checked is false we
   * display all properties including those that come from UA stylesheets.
   */
  refreshSourceFilter: function () {
    this._matchedProperties = null;
    this._sourceFilter = this.includeBrowserStyles ?
                                 CssLogic.FILTER.UA :
                                 CssLogic.FILTER.USER;
  },

  _onSourcePrefChanged: function () {
    for (let propView of this.propertyViews) {
      propView.updateSourceLinks();
    }
    this.inspector.emit("computed-view-sourcelinks-updated");
  },

  /**
   * The CSS as displayed by the UI.
   */
  createStyleViews: function () {
    if (CssComputedView.propertyNames) {
      return;
    }

    CssComputedView.propertyNames = [];

    // Here we build and cache a list of css properties supported by the browser
    // We could use any element but let's use the main document's root element
    let styles = this.styleWindow
      .getComputedStyle(this.styleDocument.documentElement);
    let mozProps = [];
    for (let i = 0, numStyles = styles.length; i < numStyles; i++) {
      let prop = styles.item(i);
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

    this._createPropertyViews().then(null, e => {
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
  focusWindow: function () {
    let win = this.styleDocument.defaultView;
    win.focus();
  },

  /**
   * Context menu handler.
   */
  _onContextMenu: function (event) {
    this._contextmenu.show(event);
  },

  _onClick: function (event) {
    let target = event.target;

    if (target.nodeName === "a") {
      event.stopPropagation();
      event.preventDefault();
      let browserWin = this.inspector.target.tab.ownerDocument.defaultView;
      browserWin.openUILinkIn(target.href, "tab");
    }
  },

  /**
   * Callback for copy event. Copy selected text.
   *
   * @param {Event} event
   *        copy event object.
   */
  _onCopy: function (event) {
    this.copySelection();
    event.preventDefault();
  },

  /**
   * Copy the current selection to the clipboard
   */
  copySelection: function () {
    try {
      let win = this.styleDocument.defaultView;
      let text = win.getSelection().toString().trim();

      // Tidy up block headings by moving CSS property names and their
      // values onto the same line and inserting a colon between them.
      let textArray = text.split(/[\r\n]+/);
      let result = "";

      // Parse text array to output string.
      if (textArray.length > 1) {
        for (let prop of textArray) {
          if (CssComputedView.propertyNames.indexOf(prop) !== -1) {
            // Property name
            result += prop;
          } else {
            // Property value
            result += ": " + prop + ";\n";
          }
        }
      } else {
        // Short text fragment.
        result = textArray[0];
      }

      clipboardHelper.copyString(result);
    } catch (e) {
      console.error(e);
    }
  },

  /**
   * Destructor for CssComputedView.
   */
  destroy: function () {
    this._viewedElement = null;
    this._outputParser = null;

    gDevTools.off("pref-changed", this._handlePrefChange);

    this._prefObserver.off(PREF_ORIG_SOURCES, this._onSourcePrefChanged);
    this._prefObserver.destroy();

    // Cancel tree construction
    if (this._createViewsProcess) {
      this._createViewsProcess.cancel();
    }
    if (this._refreshProcess) {
      this._refreshProcess.cancel();
    }

    // Remove context menu
    if (this._contextmenu) {
      this._contextmenu.destroy();
      this._contextmenu = null;
    }

    this.tooltips.destroy();
    this.highlighters.destroy();

    // Remove bound listeners
    this.styleDocument.removeEventListener("mousedown", this.focusWindow);
    this.element.removeEventListener("click", this._onClick);
    this.element.removeEventListener("copy", this._onCopy);
    this.element.removeEventListener("contextmenu", this._onContextMenu);
    this.searchField.removeEventListener("input", this._onFilterStyles);
    this.searchField.removeEventListener("contextmenu",
                                         this._onFilterTextboxContextMenu);
    this.searchClearButton.removeEventListener("click", this._onClearSearch);
    this.includeBrowserStylesCheckbox.removeEventListener("input",
      this._onIncludeBrowserStyles);

    // Nodes used in templating
    this.element = null;
    this.panel = null;
    this.searchField = null;
    this.searchClearButton = null;
    this.includeBrowserStylesCheckbox = null;

    // Property views
    for (let propView of this.propertyViews) {
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
      let value = this.tree._computed[this.name].value;
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
  this.getRTLAttr = tree.getRTLAttr;

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

    let searchTerm = this.tree.searchField.value.toLowerCase();
    let isValidSearchTerm = searchTerm.trim().length > 0;
    if (isValidSearchTerm &&
        this.name.toLowerCase().indexOf(searchTerm) === -1 &&
        this.value.toLowerCase().indexOf(searchTerm) === -1) {
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
      let isDark = this.tree._darkStripe = !this.tree._darkStripe;
      return isDark ? "property-view row-striped" : "property-view";
    }
    return "property-view-hidden";
  },

  /**
   * Returns the className that should be assigned to the propertyView content
   * container.
   *
   * @return {String}
   */
  get propertyContentClassName() {
    if (this.visible) {
      let isDark = this.tree._darkStripe;
      return isDark ? "property-content row-striped" : "property-content";
    }
    return "property-content-hidden";
  },

  /**
   * Build the markup for on computed style
   *
   * @return {Element}
   */
  buildMain: function () {
    let doc = this.tree.styleDocument;

    // Build the container element
    this.onMatchedToggle = this.onMatchedToggle.bind(this);
    this.element = doc.createElementNS(HTML_NS, "div");
    this.element.setAttribute("class", this.propertyHeaderClassName);
    this.element.addEventListener("dblclick", this.onMatchedToggle, false);

    // Make it keyboard navigable
    this.element.setAttribute("tabindex", "0");
    this.shortcuts = new KeyShortcuts({
      window: this.tree.styleWindow,
      target: this.element
    });
    this.shortcuts.on("F1", (name, event) => {
      this.mdnLinkClick(event);
    });
    this.shortcuts.on("Return", (name, event) => this.onMatchedToggle(event));
    this.shortcuts.on("Space", (name, event) => this.onMatchedToggle(event));

    let nameContainer = doc.createElementNS(HTML_NS, "div");
    nameContainer.className = "property-name-container";
    this.element.appendChild(nameContainer);

    // Build the twisty expand/collapse
    this.matchedExpander = doc.createElementNS(HTML_NS, "div");
    this.matchedExpander.className = "expander theme-twisty";
    this.matchedExpander.addEventListener("click", this.onMatchedToggle, false);
    nameContainer.appendChild(this.matchedExpander);

    // Build the style name element
    this.nameNode = doc.createElementNS(HTML_NS, "div");
    this.nameNode.setAttribute("class", "property-name theme-fg-color5");
    // Reset its tabindex attribute otherwise, if an ellipsis is applied
    // it will be reachable via TABing
    this.nameNode.setAttribute("tabindex", "");
    this.nameNode.textContent = this.nameNode.title = this.name;
    // Make it hand over the focus to the container
    this.onFocus = () => this.element.focus();
    this.nameNode.addEventListener("click", this.onFocus, false);
    nameContainer.appendChild(this.nameNode);

    let valueContainer = doc.createElementNS(HTML_NS, "div");
    valueContainer.className = "property-value-container";
    this.element.appendChild(valueContainer);

    // Build the style value element
    this.valueNode = doc.createElementNS(HTML_NS, "div");
    this.valueNode.setAttribute("class", "property-value theme-fg-color1");
    // Reset its tabindex attribute otherwise, if an ellipsis is applied
    // it will be reachable via TABing
    this.valueNode.setAttribute("tabindex", "");
    this.valueNode.setAttribute("dir", "ltr");
    // Make it hand over the focus to the container
    this.valueNode.addEventListener("click", this.onFocus, false);
    valueContainer.appendChild(this.valueNode);

    return this.element;
  },

  buildSelectorContainer: function () {
    let doc = this.tree.styleDocument;
    let element = doc.createElementNS(HTML_NS, "div");
    element.setAttribute("class", this.propertyContentClassName);
    this.matchedSelectorsContainer = doc.createElementNS(HTML_NS, "div");
    this.matchedSelectorsContainer.setAttribute("class", "matchedselectors");
    element.appendChild(this.matchedSelectorsContainer);

    return element;
  },

  /**
   * Refresh the panel's CSS property value.
   */
  refresh: function () {
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

    let outputParser = this.tree._outputParser;
    let frag = outputParser.parseCssProperty(this.propertyInfo.name,
      this.propertyInfo.value,
      {
        colorSwatchClass: "computedview-colorswatch",
        colorClass: "computedview-color",
        urlClass: "theme-link"
        // No need to use baseURI here as computed URIs are never relative.
      });
    this.valueNode.innerHTML = "";
    this.valueNode.appendChild(frag);

    this.refreshMatchedSelectors();
  },

  /**
   * Refresh the panel matched rules.
   */
  refreshMatchedSelectors: function () {
    let hasMatchedSelectors = this.hasMatchedSelectors;
    this.matchedSelectorsContainer.parentNode.hidden = !hasMatchedSelectors;

    if (hasMatchedSelectors) {
      this.matchedExpander.classList.add("expandable");
    } else {
      this.matchedExpander.classList.remove("expandable");
    }

    if (this.matchedExpanded && hasMatchedSelectors) {
      return this.tree.pageStyle
        .getMatchedSelectors(this.tree._viewedElement, this.name)
        .then(matched => {
          if (!this.matchedExpanded) {
            return promise.resolve(undefined);
          }

          this._matchedSelectorResponse = matched;

          return this._buildMatchedSelectors().then(() => {
            this.matchedExpander.setAttribute("open", "");
            this.tree.inspector.emit("computed-view-property-expanded");
          });
        }).then(null, console.error);
    }

    this.matchedSelectorsContainer.innerHTML = "";
    this.matchedExpander.removeAttribute("open");
    this.tree.inspector.emit("computed-view-property-collapsed");
    return promise.resolve(undefined);
  },

  get matchedSelectors() {
    return this._matchedSelectorResponse;
  },

  _buildMatchedSelectors: function () {
    let promises = [];
    let frag = this.element.ownerDocument.createDocumentFragment();

    for (let selector of this.matchedSelectorViews) {
      let p = createChild(frag, "p");
      let span = createChild(p, "span", {
        class: "rule-link"
      });
      let link = createChild(span, "a", {
        target: "_blank",
        class: "link theme-link",
        title: selector.href,
        sourcelocation: selector.source,
        tabindex: "0",
        textContent: selector.source
      });
      link.addEventListener("click", selector.openStyleEditor, false);
      let shortcuts = new KeyShortcuts({
        window: this.tree.styleWindow,
        target: link
      });
      shortcuts.on("Return", () => selector.openStyleEditor());

      let status = createChild(p, "span", {
        dir: "ltr",
        class: "rule-text theme-fg-color3 " + selector.statusClass,
        title: selector.statusText,
        textContent: selector.sourceText
      });
      let valueSpan = createChild(status, "span", {
        class: "other-property-value theme-fg-color1"
      });
      valueSpan.appendChild(selector.outputFragment);
      promises.push(selector.ready);
    }

    this.matchedSelectorsContainer.innerHTML = "";
    this.matchedSelectorsContainer.appendChild(frag);
    return promise.all(promises);
  },

  /**
   * Provide access to the matched SelectorViews that we are currently
   * displaying.
   */
  get matchedSelectorViews() {
    if (!this._matchedSelectorViews) {
      this._matchedSelectorViews = [];
      this._matchedSelectorResponse.forEach(selectorInfo => {
        let selectorView = new SelectorView(this.tree, selectorInfo);
        this._matchedSelectorViews.push(selectorView);
      }, this);
    }
    return this._matchedSelectorViews;
  },

  /**
   * Update all the selector source links to reflect whether we're linking to
   * original sources (e.g. Sass files).
   */
  updateSourceLinks: function () {
    if (!this._matchedSelectorViews) {
      return;
    }
    for (let view of this._matchedSelectorViews) {
      view.updateSourceLink();
    }
  },

  /**
   * The action when a user expands matched selectors.
   *
   * @param {Event} event
   *        Used to determine the class name of the targets click
   *        event.
   */
  onMatchedToggle: function (event) {
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
  mdnLinkClick: function (event) {
    let inspector = this.tree.inspector;

    if (inspector.target.tab) {
      let browserWin = inspector.target.tab.ownerDocument.defaultView;
      browserWin.openUILinkIn(this.link, "tab");
    }
    event.preventDefault();
    event.stopPropagation();
  },

  /**
   * Destroy this property view, removing event listeners
   */
  destroy: function () {
    this.element.removeEventListener("dblclick", this.onMatchedToggle, false);
    this.shortcuts.destroy();
    this.element = null;

    this.matchedExpander.removeEventListener("click", this.onMatchedToggle,
                                             false);
    this.matchedExpander = null;

    this.nameNode.removeEventListener("click", this.onFocus, false);
    this.nameNode = null;

    this.valueNode.removeEventListener("click", this.onFocus, false);
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

  this.ready = this.updateSourceLink();
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
  _cacheStatusNames: function () {
    if (SelectorView.STATUS_NAMES.length) {
      return;
    }

    for (let status in CssLogic.STATUS) {
      let i = CssLogic.STATUS[status];
      if (i > CssLogic.STATUS.UNMATCHED) {
        let value = CssComputedView.l10n("rule.status." + status);
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
    let sheet = this.selectorInfo.rule.parentStyleSheet;
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
    let outputParser = this.tree._outputParser;
    let frag = outputParser.parseCssProperty(
      this.selectorInfo.name,
      this.selectorInfo.value, {
        colorSwatchClass: "computedview-colorswatch",
        colorClass: "computedview-color",
        urlClass: "theme-link",
        baseURI: this.selectorInfo.rule.href
      }
    );
    return frag;
  },

  /**
   * Update the text of the source link to reflect whether we're showing
   * original sources or not.
   */
  updateSourceLink: function () {
    return this.updateSource().then((oldSource) => {
      if (oldSource !== this.source && this.tree.element) {
        let selector = '[sourcelocation="' + oldSource + '"]';
        let link = this.tree.element.querySelector(selector);
        if (link) {
          link.textContent = this.source;
          link.setAttribute("sourcelocation", this.source);
        }
      }
    });
  },

  /**
   * Update the 'source' store based on our original sources preference.
   */
  updateSource: function () {
    let rule = this.selectorInfo.rule;
    this.sheet = rule.parentStyleSheet;

    if (!rule || !this.sheet) {
      let oldSource = this.source;
      this.source = CssLogic.l10n("rule.sourceElement");
      return promise.resolve(oldSource);
    }

    let showOrig = Services.prefs.getBoolPref(PREF_ORIG_SOURCES);

    if (showOrig && rule.type !== ELEMENT_STYLE) {
      let deferred = defer();

      // set as this first so we show something while we're fetching
      this.source = CssLogic.shortSource(this.sheet) + ":" + rule.line;

      rule.getOriginalLocation().then(({href, line}) => {
        let oldSource = this.source;
        this.source = CssLogic.shortSource({href: href}) + ":" + line;
        deferred.resolve(oldSource);
      });

      return deferred.promise;
    }

    let oldSource = this.source;
    this.source = CssLogic.shortSource(this.sheet) + ":" + rule.line;
    return promise.resolve(oldSource);
  },

  /**
   * When a css link is clicked this method is called in order to either:
   *   1. Open the link in view source (for chrome stylesheets).
   *   2. Open the link in the style editor.
   *
   *   We can only view stylesheets contained in document.styleSheets inside the
   *   style editor.
   */
  openStyleEditor: function () {
    let inspector = this.tree.inspector;
    let rule = this.selectorInfo.rule;

    // The style editor can only display stylesheets coming from content because
    // chrome stylesheets are not listed in the editor's stylesheet selector.
    //
    // If the stylesheet is a content stylesheet we send it to the style
    // editor else we display it in the view source window.
    let parentStyleSheet = rule.parentStyleSheet;
    if (!parentStyleSheet || parentStyleSheet.isSystem) {
      let toolbox = gDevTools.getToolbox(inspector.target);
      toolbox.viewSource(rule.href, rule.line);
      return;
    }

    let location = promise.resolve(rule.location);
    if (Services.prefs.getBoolPref(PREF_ORIG_SOURCES)) {
      location = rule.getOriginalLocation();
    }

    location.then(({source, href, line, column}) => {
      let target = inspector.target;
      if (ToolDefinitions.styleEditor.isTargetSupported(target)) {
        gDevTools.showToolbox(target, "styleeditor").then(function (toolbox) {
          let sheet = source || href;
          toolbox.getCurrentPanel().selectStyleSheet(sheet, line, column);
        });
      }
    });
  }
};

function ComputedViewTool(inspector, window) {
  this.inspector = inspector;
  this.document = window.document;

  this.computedView = new CssComputedView(this.inspector, this.document,
    this.inspector.pageStyle);
  this.layoutView = new LayoutView(this.inspector, this.document);

  this.onSelected = this.onSelected.bind(this);
  this.refresh = this.refresh.bind(this);
  this.onPanelSelected = this.onPanelSelected.bind(this);
  this.onMutations = this.onMutations.bind(this);
  this.onResized = this.onResized.bind(this);

  this.inspector.selection.on("detached", this.onSelected);
  this.inspector.selection.on("new-node-front", this.onSelected);
  this.inspector.selection.on("pseudoclass", this.refresh);
  this.inspector.sidebar.on("computedview-selected", this.onPanelSelected);
  this.inspector.pageStyle.on("stylesheet-updated", this.refresh);
  this.inspector.walker.on("mutations", this.onMutations);
  this.inspector.walker.on("resize", this.onResized);

  this.computedView.selectElement(null);

  this.onSelected();
}

ComputedViewTool.prototype = {
  isSidebarActive: function () {
    if (!this.computedView) {
      return false;
    }
    return this.inspector.sidebar.getCurrentTabID() == "computedview";
  },

  onSelected: function (event) {
    // Ignore the event if the view has been destroyed, or if it's inactive.
    // But only if the current selection isn't null. If it's been set to null,
    // let the update go through as this is needed to empty the view on
    // navigation.
    if (!this.computedView) {
      return;
    }

    let isInactive = !this.isSidebarActive() &&
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

    if (!event || event == "new-node-front") {
      let done = this.inspector.updating("computed-view");
      this.computedView.selectElement(this.inspector.selection.nodeFront).then(() => {
        done();
      });
    }
  },

  refresh: function () {
    if (this.isSidebarActive()) {
      this.computedView.refreshPanel();
    }
  },

  onPanelSelected: function () {
    if (this.inspector.selection.nodeFront === this.computedView._viewedElement) {
      this.refresh();
    } else {
      this.onSelected();
    }
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
    this.inspector.sidebar.off("computedview-selected", this.refresh);
    this.inspector.selection.off("pseudoclass", this.refresh);
    this.inspector.selection.off("new-node-front", this.onSelected);
    this.inspector.selection.off("detached", this.onSelected);
    this.inspector.sidebar.off("computedview-selected", this.onPanelSelected);
    if (this.inspector.pageStyle) {
      this.inspector.pageStyle.off("stylesheet-updated", this.refresh);
    }

    this.computedView.destroy();
    this.layoutView.destroy();

    this.computedView = this.layoutView = this.document = this.inspector = null;
  }
};

exports.CssComputedView = CssComputedView;
exports.ComputedViewTool = ComputedViewTool;
exports.PropertyView = PropertyView;
