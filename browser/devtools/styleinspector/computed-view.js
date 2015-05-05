/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const {Cc, Ci, Cu} = require("chrome");

const ToolDefinitions = require("main").Tools;
const {CssLogic} = require("devtools/styleinspector/css-logic");
const {ELEMENT_STYLE} = require("devtools/server/actors/styles");
const {Promise: promise} = Cu.import("resource://gre/modules/Promise.jsm", {});
const {EventEmitter} = require("devtools/toolkit/event-emitter");
const {OutputParser} = require("devtools/output-parser");
const {PrefObserver, PREF_ORIG_SOURCES} = require("devtools/styleeditor/utils");
const {gDevTools} = Cu.import("resource:///modules/devtools/gDevTools.jsm", {});
const overlays = require("devtools/styleinspector/style-inspector-overlays");

Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource://gre/modules/XPCOMUtils.jsm");
Cu.import("resource://gre/modules/devtools/Templater.jsm");

XPCOMUtils.defineLazyModuleGetter(this, "PluralForm",
                                  "resource://gre/modules/PluralForm.jsm");

const FILTER_CHANGED_TIMEOUT = 150;
const HTML_NS = "http://www.w3.org/1999/xhtml";
const XUL_NS = "http://www.mozilla.org/keymaster/gatekeeper/there.is.only.xul";

/**
 * Helper for long-running processes that should yield occasionally to
 * the mainloop.
 *
 * @param {Window} aWin
 *        Timeouts will be set on this window when appropriate.
 * @param {Generator} aGenerator
 *        Will iterate this generator.
 * @param {object} aOptions
 *        Options for the update process:
 *          onItem {function} Will be called with the value of each iteration.
 *          onBatch {function} Will be called after each batch of iterations,
 *            before yielding to the main loop.
 *          onDone {function} Will be called when iteration is complete.
 *          onCancel {function} Will be called if the process is canceled.
 *          threshold {int} How long to process before yielding, in ms.
 *
 * @constructor
 */
function UpdateProcess(aWin, aGenerator, aOptions)
{
  this.win = aWin;
  this.iter = _Iterator(aGenerator);
  this.onItem = aOptions.onItem || function() {};
  this.onBatch = aOptions.onBatch || function () {};
  this.onDone = aOptions.onDone || function() {};
  this.onCancel = aOptions.onCancel || function() {};
  this.threshold = aOptions.threshold || 45;

  this.canceled = false;
}

UpdateProcess.prototype = {
  /**
   * Schedule a new batch on the main loop.
   */
  schedule: function UP_schedule()
  {
    if (this.canceled) {
      return;
    }
    this._timeout = this.win.setTimeout(this._timeoutHandler.bind(this), 0);
  },

  /**
   * Cancel the running process.  onItem will not be called again,
   * and onCancel will be called.
   */
  cancel: function UP_cancel()
  {
    if (this._timeout) {
      this.win.clearTimeout(this._timeout);
      this._timeout = 0;
    }
    this.canceled = true;
    this.onCancel();
  },

  _timeoutHandler: function UP_timeoutHandler() {
    this._timeout = null;
    try {
      this._runBatch();
      this.schedule();
    } catch(e) {
      if (e instanceof StopIteration) {
        this.onBatch();
        this.onDone();
        return;
      }
      console.error(e);
      throw e;
    }
  },

  _runBatch: function Y_runBatch()
  {
    let time = Date.now();
    while(!this.canceled) {
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
 * CssHtmlTree is a panel that manages the display of a table sorted by style.
 * There should be one instance of CssHtmlTree per style display (of which there
 * will generally only be one).
 *
 * @params {StyleInspector} aStyleInspector The owner of this CssHtmlTree
 * @param {PageStyleFront} aPageStyle
 *        Front for the page style actor that will be providing
 *        the style information.
 *
 * @constructor
 */
function CssHtmlTree(aStyleInspector, aPageStyle)
{
  this.styleWindow = aStyleInspector.doc.defaultView;
  this.styleDocument = aStyleInspector.doc;
  this.styleInspector = aStyleInspector;
  this.inspector = this.styleInspector.inspector;
  this.pageStyle = aPageStyle;
  this.propertyViews = [];

  this._outputParser = new OutputParser();

  let chromeReg = Cc["@mozilla.org/chrome/chrome-registry;1"].
    getService(Ci.nsIXULChromeRegistry);
  this.getRTLAttr = chromeReg.isLocaleRTL("global") ? "rtl" : "ltr";

  // Create bound methods.
  this.focusWindow = this.focusWindow.bind(this);
  this._onContextMenu = this._onContextMenu.bind(this);
  this._contextMenuUpdate = this._contextMenuUpdate.bind(this);
  this._onSelectAll = this._onSelectAll.bind(this);
  this._onClick = this._onClick.bind(this);
  this._onCopy = this._onCopy.bind(this);
  this._onCopyColor = this._onCopyColor.bind(this);
  this._onFilterStyles = this._onFilterStyles.bind(this);
  this._onFilterKeyPress = this._onFilterKeyPress.bind(this);
  this._onClearSearch = this._onClearSearch.bind(this);
  this._onIncludeBrowserStyles = this._onIncludeBrowserStyles.bind(this);
  this._onFilterTextboxContextMenu = this._onFilterTextboxContextMenu.bind(this);

  let doc = this.styleDocument;
  this.root = doc.getElementById("root");
  this.element = doc.getElementById("propertyContainer");
  this.searchField = doc.getElementById("computedview-searchbox");
  this.searchClearButton = doc.getElementById("computedview-searchinput-clear");
  this.includeBrowserStylesCheckbox = doc.getElementById("browser-style-checkbox");

  this.styleDocument.addEventListener("mousedown", this.focusWindow);
  this.element.addEventListener("click", this._onClick);
  this.element.addEventListener("copy", this._onCopy);
  this.element.addEventListener("contextmenu", this._onContextMenu);
  this.searchField.addEventListener("input", this._onFilterStyles);
  this.searchField.addEventListener("keypress", this._onFilterKeyPress);
  this.searchField.addEventListener("contextmenu", this._onFilterTextboxContextMenu);
  this.searchClearButton.addEventListener("click", this._onClearSearch);
  this.includeBrowserStylesCheckbox.addEventListener("command",
    this._onIncludeBrowserStyles);

  this.searchClearButton.hidden = true;

  // No results text.
  this.noResults = this.styleDocument.getElementById("noResults");

  // Refresh panel when color unit changed.
  this._handlePrefChange = this._handlePrefChange.bind(this);
  gDevTools.on("pref-changed", this._handlePrefChange);

  // Refresh panel when pref for showing original sources changes
  this._updateSourceLinks = this._updateSourceLinks.bind(this);
  this._prefObserver = new PrefObserver("devtools.");
  this._prefObserver.on(PREF_ORIG_SOURCES, this._updateSourceLinks);

  // The element that we're inspecting, and the document that it comes from.
  this.viewedElement = null;

  this._buildContextMenu();
  this.createStyleViews();

  // Add the tooltips and highlightersoverlay
  this.tooltips = new overlays.TooltipsOverlay(this);
  this.tooltips.addToView();
  this.highlighters = new overlays.HighlightersOverlay(this);
  this.highlighters.addToView();
}

/**
 * Memoized lookup of a l10n string from a string bundle.
 * @param {string} aName The key to lookup.
 * @returns A localized version of the given key.
 */
CssHtmlTree.l10n = function CssHtmlTree_l10n(aName)
{
  try {
    return CssHtmlTree._strings.GetStringFromName(aName);
  } catch (ex) {
    Services.console.logStringMessage("Error reading '" + aName + "'");
    throw new Error("l10n error with " + aName);
  }
};

/**
 * Clone the given template node, and process it by resolving ${} references
 * in the template.
 *
 * @param {nsIDOMElement} aTemplate the template note to use.
 * @param {nsIDOMElement} aDestination the destination node where the
 * processed nodes will be displayed.
 * @param {object} aData the data to pass to the template.
 * @param {Boolean} aPreserveDestination If true then the template will be
 * appended to aDestination's content else aDestination.innerHTML will be
 * cleared before the template is appended.
 */
CssHtmlTree.processTemplate = function CssHtmlTree_processTemplate(aTemplate,
                                  aDestination, aData, aPreserveDestination)
{
  if (!aPreserveDestination) {
    aDestination.innerHTML = "";
  }

  // All the templater does is to populate a given DOM tree with the given
  // values, so we need to clone the template first.
  let duplicated = aTemplate.cloneNode(true);

  // See https://github.com/mozilla/domtemplate/blob/master/README.md
  // for docs on the template() function
  template(duplicated, aData, { allowEval: true });
  while (duplicated.firstChild) {
    aDestination.appendChild(duplicated.firstChild);
  }
};

XPCOMUtils.defineLazyGetter(CssHtmlTree, "_strings", function() Services.strings
        .createBundle("chrome://global/locale/devtools/styleinspector.properties"));

XPCOMUtils.defineLazyGetter(this, "clipboardHelper", function() {
  return Cc["@mozilla.org/widget/clipboardhelper;1"].
    getService(Ci.nsIClipboardHelper);
});

CssHtmlTree.prototype = {
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

  setPageStyle: function(pageStyle) {
    this.pageStyle = pageStyle;
  },

  get includeBrowserStyles()
  {
    return this.includeBrowserStylesCheckbox.checked;
  },

  _handlePrefChange: function(event, data) {
    if (this._computed && (data.pref == "devtools.defaultColorUnit" ||
        data.pref == PREF_ORIG_SOURCES)) {
      this.refreshPanel();
    }
  },

  /**
   * Update the view with a new selected element.
   * The CssHtmlTree panel will show the style information for the given element.
   * @param {NodeFront} aElement The highlighted node to get styles for.
   * @returns a promise that will be resolved when highlighting is complete.
   */
  selectElement: function(aElement) {
    if (!aElement) {
      this.viewedElement = null;
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

    if (aElement === this.viewedElement) {
      return promise.resolve(undefined);
    }

    this.viewedElement = aElement;
    this.refreshSourceFilter();

    return this.refreshPanel();
  },

  /**
   * Get the type of a given node in the computed-view
   * @param {DOMNode} node The node which we want information about
   * @return {Object} The type information object contains the following props:
   * - type {String} One of the VIEW_NODE_XXX_TYPE const in
   *   style-inspector-overlays
   * - value {Object} Depends on the type of the node
   * returns null of the node isn't anything we care about
   */
  getNodeInfo: function(node) {
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
      }
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

  _createPropertyViews: function()
  {
    if (this._createViewsPromise) {
      return this._createViewsPromise;
    }

    let deferred = promise.defer();
    this._createViewsPromise = deferred.promise;

    this.refreshSourceFilter();
    this.numVisibleProperties = 0;
    let fragment = this.styleDocument.createDocumentFragment();

    this._createViewsProcess = new UpdateProcess(this.styleWindow, CssHtmlTree.propertyNames, {
      onItem: (aPropertyName) => {
        // Per-item callback.
        let propView = new PropertyView(this, aPropertyName);
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
    });

    this._createViewsProcess.schedule();
    return deferred.promise;
  },

  /**
   * Refresh the panel content.
   */
  refreshPanel: function CssHtmlTree_refreshPanel()
  {
    if (!this.viewedElement) {
      return promise.resolve();
    }

    // Capture the current viewed element to return from the promise handler
    // early if it changed
    let viewedElement = this.viewedElement;

    return promise.all([
      this._createPropertyViews(),
      this.pageStyle.getComputed(this.viewedElement, {
        filter: this._sourceFilter,
        onlyMatched: !this.includeBrowserStyles,
        markMatched: true
      })
    ]).then(([createViews, computed]) => {
      if (viewedElement !== this.viewedElement) {
        return;
      }

      this._matchedProperties = new Set;
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

      let deferred = promise.defer();
      this._refreshProcess = new UpdateProcess(this.styleWindow, this.propertyViews, {
        onItem: (aPropView) => {
          aPropView.refresh();
        },
        onDone: () => {
          this._refreshProcess = null;
          this.noResults.hidden = this.numVisibleProperties > 0;

          if (this.searchField.value.length > 0 && !this.numVisibleProperties) {
            this.searchField.classList.add("devtools-style-searchbox-no-match");
          } else {
            this.searchField.classList.remove("devtools-style-searchbox-no-match");
          }

          this.inspector.emit("computed-view-refreshed");
          deferred.resolve(undefined);
        }
      });
      this._refreshProcess.schedule();
      return deferred.promise;
    }).then(null, (err) => console.error(err));
  },

  /**
   * Called when the user enters a search term in the filter style search box.
   *
   * @param {Event} aEvent the DOM Event object.
   */
  _onFilterStyles: function(aEvent)
  {
    let win = this.styleWindow;

    if (this._filterChangedTimeout) {
      win.clearTimeout(this._filterChangedTimeout);
    }

    let filterTimeout = (this.searchField.value.length > 0)
      ? FILTER_CHANGED_TIMEOUT : 0;
    this.searchClearButton.hidden = this.searchField.value.length === 0;

    this._filterChangedTimeout = win.setTimeout(() => {
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
   * Handle the search box's keypress event. If the escape key is pressed,
   * clear the search box field.
   */
  _onFilterKeyPress: function(aEvent) {
    if (aEvent.keyCode === Ci.nsIDOMKeyEvent.DOM_VK_ESCAPE &&
        this._onClearSearch()) {
      aEvent.preventDefault();
      aEvent.stopPropagation();
    }
  },

  /**
   * Context menu handler for filter style search box.
   */
  _onFilterTextboxContextMenu: function(event) {
    try {
      this.styleDocument.defaultView.focus();
      let contextmenu = this.inspector.toolbox.textboxContextMenuPopup;
      contextmenu.openPopupAtScreen(event.screenX, event.screenY, true);
    } catch(e) {
      console.error(e);
    }
  },

  /**
   * Called when the user clicks on the clear button in the filter style search
   * box. Returns true if the search box is cleared and false otherwise.
   */
  _onClearSearch: function() {
    if (this.searchField.value) {
      this.searchField.value = "";
      this.searchField.focus();
      this._onFilterStyles();
      return true;
    }

    return false;
  },

  /**
   * The change event handler for the includeBrowserStyles checkbox.
   *
   * @param {Event} aEvent the DOM Event object.
   */
  _onIncludeBrowserStyles: function(aEvent)
  {
    this.refreshSourceFilter();
    this.refreshPanel();
  },

  /**
   * When includeBrowserStylesCheckbox.checked is false we only display
   * properties that have matched selectors and have been included by the
   * document or one of thedocument's stylesheets. If .checked is false we
   * display all properties including those that come from UA stylesheets.
   */
  refreshSourceFilter: function CssHtmlTree_setSourceFilter()
  {
    this._matchedProperties = null;
    this._sourceFilter = this.includeBrowserStyles ?
                                 CssLogic.FILTER.UA :
                                 CssLogic.FILTER.USER;
  },

  _updateSourceLinks: function CssHtmlTree__updateSourceLinks()
  {
    for (let propView of this.propertyViews) {
      propView.updateSourceLinks();
    }
    this.inspector.emit("computed-view-sourcelinks-updated");
  },

  /**
   * The CSS as displayed by the UI.
   */
  createStyleViews: function CssHtmlTree_createStyleViews()
  {
    if (CssHtmlTree.propertyNames) {
      return;
    }

    CssHtmlTree.propertyNames = [];

    // Here we build and cache a list of css properties supported by the browser
    // We could use any element but let's use the main document's root element
    let styles = this.styleWindow.getComputedStyle(this.styleDocument.documentElement);
    let mozProps = [];
    for (let i = 0, numStyles = styles.length; i < numStyles; i++) {
      let prop = styles.item(i);
      if (prop.startsWith("--")) {
        // Skip any CSS variables used inside of browser CSS files
        continue;
      } else if (prop.startsWith("-")) {
        mozProps.push(prop);
      } else {
        CssHtmlTree.propertyNames.push(prop);
      }
    }

    CssHtmlTree.propertyNames.sort();
    CssHtmlTree.propertyNames.push.apply(CssHtmlTree.propertyNames,
      mozProps.sort());

    this._createPropertyViews().then(null, e => {
      if (!this.styleInspector) {
        console.warn("The creation of property views was cancelled because the " +
          "computed-view was destroyed before it was done creating views");
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
  get matchedProperties()
  {
    return this._matchedProperties || new Set;
  },

  /**
   * Focus the window on mousedown.
   *
   * @param aEvent The event object
   */
  focusWindow: function(aEvent)
  {
    let win = this.styleDocument.defaultView;
    win.focus();
  },

  /**
   * Create a context menu.
   */
  _buildContextMenu: function()
  {
    let doc = this.styleDocument.defaultView.parent.document;

    this._contextmenu = this.styleDocument.createElementNS(XUL_NS, "menupopup");
    this._contextmenu.addEventListener("popupshowing", this._contextMenuUpdate);
    this._contextmenu.id = "computed-view-context-menu";

    // Select All
    this.menuitemSelectAll = createMenuItem(this._contextmenu, {
      label: "computedView.contextmenu.selectAll",
      accesskey: "computedView.contextmenu.selectAll.accessKey",
      command: this._onSelectAll
    });

    // Copy
    this.menuitemCopy = createMenuItem(this._contextmenu, {
      label: "computedView.contextmenu.copy",
      accesskey: "computedView.contextmenu.copy.accessKey",
      command: this._onCopy
    });

    // Copy color
    this.menuitemCopyColor = createMenuItem(this._contextmenu, {
      label: "ruleView.contextmenu.copyColor",
      accesskey: "ruleView.contextmenu.copyColor.accessKey",
      command: this._onCopyColor
    });

    // Show Original Sources
    this.menuitemSources= createMenuItem(this._contextmenu, {
      label: "ruleView.contextmenu.showOrigSources",
      accesskey: "ruleView.contextmenu.showOrigSources.accessKey",
      command: this._onToggleOrigSources,
      type: "checkbox"
    });

    let popupset = doc.documentElement.querySelector("popupset");
    if (!popupset) {
      popupset = doc.createElementNS(XUL_NS, "popupset");
      doc.documentElement.appendChild(popupset);
    }
    popupset.appendChild(this._contextmenu);
  },

  /**
   * Update the context menu. This means enabling or disabling menuitems as
   * appropriate.
   */
  _contextMenuUpdate: function()
  {
    let win = this.styleDocument.defaultView;
    let disable = win.getSelection().isCollapsed;
    this.menuitemCopy.disabled = disable;

    let showOrig = Services.prefs.getBoolPref(PREF_ORIG_SOURCES);
    this.menuitemSources.setAttribute("checked", showOrig);

    this.menuitemCopyColor.hidden = !this._isColorPopup();
  },

  /**
   * A helper that determines if the popup was opened with a click to a color
   * value and saves the color to this._colorToCopy.
   *
   * @return {Boolean}
   *         true if click on color opened the popup, false otherwise.
   */
  _isColorPopup: function () {
    this._colorToCopy = "";

    let trigger = this.popupNode;
    if (!trigger) {
      return false;
    }

    let container = (trigger.nodeType == trigger.TEXT_NODE) ?
                     trigger.parentElement : trigger;

    let isColorNode = el => el.dataset && "color" in el.dataset;

    while (!isColorNode(container)) {
      container = container.parentNode;
      if (!container) {
        return false;
      }
    }

    this._colorToCopy = container.dataset["color"];
    return true;
  },

  /**
   * Context menu handler.
   */
  _onContextMenu: function(event) {
    try {
      this.popupNode = event.explicitOriginalTarget;
      this.styleDocument.defaultView.focus();
      this._contextmenu.openPopupAtScreen(event.screenX, event.screenY, true);
    } catch(e) {
      console.error(e);
    }
  },

  /**
   * Select all text.
   */
  _onSelectAll: function()
  {
    try {
      let win = this.styleDocument.defaultView;
      let selection = win.getSelection();

      selection.selectAllChildren(this.styleDocument.documentElement);
    } catch(e) {
      console.error(e);
    }
  },

  _onClick: function(event) {
    let target = event.target;

    if (target.nodeName === "a") {
      event.stopPropagation();
      event.preventDefault();
      let browserWin = this.inspector.target.tab.ownerDocument.defaultView;
      browserWin.openUILinkIn(target.href, "tab");
    }
  },

  _onCopyColor: function() {
    clipboardHelper.copyString(this._colorToCopy, this.styleDocument);
  },

  /**
   * Copy selected text.
   *
   * @param event The event object
   */
  _onCopy: function(event)
  {
    try {
      let win = this.styleDocument.defaultView;
      let text = win.getSelection().toString().trim();

      // Tidy up block headings by moving CSS property names and their values onto
      // the same line and inserting a colon between them.
      let textArray = text.split(/[\r\n]+/);
      let result = "";

      // Parse text array to output string.
      if (textArray.length > 1) {
        for (let prop of textArray) {
          if (CssHtmlTree.propertyNames.indexOf(prop) !== -1) {
            // Property name
            result += prop;
          } else {
            // Property value
            result += ": " + prop;
            if (result.length > 0) {
              result += ";\n";
            }
          }
        }
      } else {
        // Short text fragment.
        result = textArray[0];
      }

      clipboardHelper.copyString(result, this.styleDocument);

      if (event) {
        event.preventDefault();
      }
    } catch(e) {
      console.error(e);
    }
  },

  /**
   *  Toggle the original sources pref.
   */
  _onToggleOrigSources: function()
  {
    let isEnabled = Services.prefs.getBoolPref(PREF_ORIG_SOURCES);
    Services.prefs.setBoolPref(PREF_ORIG_SOURCES, !isEnabled);
  },

  /**
   * Destructor for CssHtmlTree.
   */
  destroy: function CssHtmlTree_destroy()
  {
    this.viewedElement = null;
    this._outputParser = null;

    gDevTools.off("pref-changed", this._handlePrefChange);

    this._prefObserver.off(PREF_ORIG_SOURCES, this._updateSourceLinks);
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
      // Destroy the Select All menuitem.
      this.menuitemCopy.removeEventListener("command", this._onCopy);
      this.menuitemCopy = null;

      // Destroy the Copy menuitem.
      this.menuitemSelectAll.removeEventListener("command", this._onSelectAll);
      this.menuitemSelectAll = null;

      // Destroy Copy Color menuitem.
      this.menuitemCopyColor.removeEventListener("command", this._onCopyColor);
      this.menuitemCopyColor = null;

      // Destroy the context menu.
      this._contextmenu.removeEventListener("popupshowing", this._contextMenuUpdate);
      this._contextmenu.parentNode.removeChild(this._contextmenu);
      this._contextmenu = null;
    }

    this.popupNode = null;

    this.tooltips.destroy();
    this.highlighters.destroy();

    // Remove bound listeners
    this.styleDocument.removeEventListener("mousedown", this.focusWindow);
    this.element.removeEventListener("click", this._onClick);
    this.element.removeEventListener("copy", this._onCopy);
    this.element.removeEventListener("contextmenu", this._onContextMenu);
    this.searchField.removeEventListener("input", this._onFilterStyles);
    this.searchField.removeEventListener("keypress", this._onFilterKeyPress);
    this.searchField.removeEventListener("contextmenu", this._onFilterTextboxContextMenu);
    this.searchClearButton.removeEventListener("click", this._onClearSearch);
    this.includeBrowserStylesCheckbox.removeEventListener("command",
      this.includeBrowserStylesChanged);

    // Nodes used in templating
    this.root = null;
    this.element = null;
    this.panel = null;
    this.searchField = null;
    this.searchClearButton = null;
    this.includeBrowserStylesCheckbox = null;

    // The document in which we display the results (csshtmltree.xul).
    this.styleDocument = null;

    for (let propView of this.propertyViews)  {
      propView.destroy();
    }

    // The element that we're inspecting, and the document that it comes from.
    this.propertyViews = null;
    this.styleWindow = null;
    this.styleDocument = null;
    this.styleInspector = null;
  }
};

function PropertyInfo(aTree, aName) {
  this.tree = aTree;
  this.name = aName;
}
PropertyInfo.prototype = {
  get value() {
    if (this.tree._computed) {
      let value = this.tree._computed[this.name].value;
      return value;
    }
  }
};

function createMenuItem(aMenu, aAttributes)
{
  let item = aMenu.ownerDocument.createElementNS(XUL_NS, "menuitem");

  item.setAttribute("label", CssHtmlTree.l10n(aAttributes.label));
  item.setAttribute("accesskey", CssHtmlTree.l10n(aAttributes.accesskey));
  item.addEventListener("command", aAttributes.command);

  if (aAttributes.type) {
    item.setAttribute("type", aAttributes.type);
  }

  aMenu.appendChild(item);

  return item;
}

/**
 * A container to give easy access to property data from the template engine.
 *
 * @constructor
 * @param {CssHtmlTree} aTree the CssHtmlTree instance we are working with.
 * @param {string} aName the CSS property name for which this PropertyView
 * instance will render the rules.
 */
function PropertyView(aTree, aName)
{
  this.tree = aTree;
  this.name = aName;
  this.getRTLAttr = aTree.getRTLAttr;

  this.link = "https://developer.mozilla.org/CSS/" + aName;

  this._propertyInfo = new PropertyInfo(aTree, aName);
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
  prevViewedElement: null,

  /**
   * Get the computed style for the current property.
   *
   * @return {string} the computed style for the current property of the
   * currently highlighted element.
   */
  get value()
  {
    return this.propertyInfo.value;
  },

  /**
   * An easy way to access the CssPropertyInfo behind this PropertyView.
   */
  get propertyInfo()
  {
    return this._propertyInfo;
  },

  /**
   * Does the property have any matched selectors?
   */
  get hasMatchedSelectors()
  {
    return this.tree.matchedProperties.has(this.name);
  },

  /**
   * Should this property be visible?
   */
  get visible()
  {
    if (!this.tree.viewedElement) {
      return false;
    }

    if (!this.tree.includeBrowserStyles && !this.hasMatchedSelectors) {
      return false;
    }

    let searchTerm = this.tree.searchField.value.toLowerCase();
    let isValidSearchTerm = searchTerm.trim().length > 0;
    if (isValidSearchTerm &&
        this.name.toLowerCase().indexOf(searchTerm) == -1 &&
        this.value.toLowerCase().indexOf(searchTerm) == -1) {
      return false;
    }

    return true;
  },

  /**
   * Returns the className that should be assigned to the propertyView.
   * @return string
   */
  get propertyHeaderClassName()
  {
    if (this.visible) {
      let isDark = this.tree._darkStripe = !this.tree._darkStripe;
      return isDark ? "property-view row-striped" : "property-view";
    }
    return "property-view-hidden";
  },

  /**
   * Returns the className that should be assigned to the propertyView content
   * container.
   * @return string
   */
  get propertyContentClassName()
  {
    if (this.visible) {
      let isDark = this.tree._darkStripe;
      return isDark ? "property-content row-striped" : "property-content";
    }
    return "property-content-hidden";
  },

  /**
   * Build the markup for on computed style
   * @return Element
   */
  buildMain: function PropertyView_buildMain()
  {
    let doc = this.tree.styleDocument;

    // Build the container element
    this.onMatchedToggle = this.onMatchedToggle.bind(this);
    this.element = doc.createElementNS(HTML_NS, "div");
    this.element.setAttribute("class", this.propertyHeaderClassName);
    this.element.addEventListener("dblclick", this.onMatchedToggle, false);

    // Make it keyboard navigable
    this.element.setAttribute("tabindex", "0");
    this.onKeyDown = (aEvent) => {
      let keyEvent = Ci.nsIDOMKeyEvent;
      if (aEvent.keyCode == keyEvent.DOM_VK_F1) {
        this.mdnLinkClick();
      }
      if (aEvent.keyCode == keyEvent.DOM_VK_RETURN ||
        aEvent.keyCode == keyEvent.DOM_VK_SPACE) {
        this.onMatchedToggle(aEvent);
      }
    };
    this.element.addEventListener("keydown", this.onKeyDown, false);

    // Build the twisty expand/collapse
    this.matchedExpander = doc.createElementNS(HTML_NS, "div");
    this.matchedExpander.className = "expander theme-twisty";
    this.matchedExpander.addEventListener("click", this.onMatchedToggle, false);
    this.element.appendChild(this.matchedExpander);

    this.focusElement = () => this.element.focus();

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
    this.element.appendChild(this.nameNode);

    // Build the style value element
    this.valueNode = doc.createElementNS(HTML_NS, "div");
    this.valueNode.setAttribute("class", "property-value theme-fg-color1");
    // Reset its tabindex attribute otherwise, if an ellipsis is applied
    // it will be reachable via TABing
    this.valueNode.setAttribute("tabindex", "");
    this.valueNode.setAttribute("dir", "ltr");
    // Make it hand over the focus to the container
    this.valueNode.addEventListener("click", this.onFocus, false);
    this.element.appendChild(this.valueNode);

    return this.element;
  },

  buildSelectorContainer: function PropertyView_buildSelectorContainer()
  {
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
  refresh: function PropertyView_refresh()
  {
    this.element.className = this.propertyHeaderClassName;
    this.element.nextElementSibling.className = this.propertyContentClassName;

    if (this.prevViewedElement != this.tree.viewedElement) {
      this._matchedSelectorViews = null;
      this.prevViewedElement = this.tree.viewedElement;
    }

    if (!this.tree.viewedElement || !this.visible) {
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
  refreshMatchedSelectors: function PropertyView_refreshMatchedSelectors()
  {
    let hasMatchedSelectors = this.hasMatchedSelectors;
    this.matchedSelectorsContainer.parentNode.hidden = !hasMatchedSelectors;

    if (hasMatchedSelectors) {
      this.matchedExpander.classList.add("expandable");
    } else {
      this.matchedExpander.classList.remove("expandable");
    }

    if (this.matchedExpanded && hasMatchedSelectors) {
      return this.tree.pageStyle.getMatchedSelectors(this.tree.viewedElement, this.name).then(matched => {
        if (!this.matchedExpanded) {
          return;
        }

        this._matchedSelectorResponse = matched;

        this._buildMatchedSelectors();
        this.matchedExpander.setAttribute("open", "");

        this.tree.inspector.emit("computed-view-property-expanded");
      }).then(null, console.error);
    } else {
      this.matchedSelectorsContainer.innerHTML = "";
      this.matchedExpander.removeAttribute("open");
      this.tree.inspector.emit("computed-view-property-collapsed");
      return promise.resolve(undefined);
    }
  },

  get matchedSelectors()
  {
    return this._matchedSelectorResponse;
  },

  _buildMatchedSelectors: function() {
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
      link.addEventListener("keydown", selector.maybeOpenStyleEditor, false);

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
    }

    this.matchedSelectorsContainer.appendChild(frag);
  },

  /**
   * Provide access to the matched SelectorViews that we are currently
   * displaying.
   */
  get matchedSelectorViews()
  {
    if (!this._matchedSelectorViews) {
      this._matchedSelectorViews = [];
      this._matchedSelectorResponse.forEach(
        function matchedSelectorViews_convert(aSelectorInfo) {
          this._matchedSelectorViews.push(new SelectorView(this.tree, aSelectorInfo));
        }, this);
    }

    return this._matchedSelectorViews;
  },

  /**
   * Update all the selector source links to reflect whether we're linking to
   * original sources (e.g. Sass files).
   */
  updateSourceLinks: function PropertyView_updateSourceLinks()
  {
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
   * @param {Event} aEvent Used to determine the class name of the targets click
   * event.
   */
  onMatchedToggle: function PropertyView_onMatchedToggle(aEvent)
  {
    if (aEvent.shiftKey) {
      return;
    }
    this.matchedExpanded = !this.matchedExpanded;
    this.refreshMatchedSelectors();
    aEvent.preventDefault();
  },

  /**
   * The action when a user clicks on the MDN help link for a property.
   */
  mdnLinkClick: function PropertyView_mdnLinkClick(aEvent)
  {
    let inspector = this.tree.inspector;

    if (inspector.target.tab) {
      let browserWin = inspector.target.tab.ownerDocument.defaultView;
      browserWin.openUILinkIn(this.link, "tab");
    }
    aEvent.preventDefault();
  },

  /**
   * Destroy this property view, removing event listeners
   */
  destroy: function PropertyView_destroy() {
    this.element.removeEventListener("dblclick", this.onMatchedToggle, false);
    this.element.removeEventListener("keydown", this.onKeyDown, false);
    this.element = null;

    this.matchedExpander.removeEventListener("click", this.onMatchedToggle, false);
    this.matchedExpander = null;

    this.nameNode.removeEventListener("click", this.onFocus, false);
    this.nameNode = null;

    this.valueNode.removeEventListener("click", this.onFocus, false);
    this.valueNode = null;
  }
};

/**
 * A container to give us easy access to display data from a CssRule
 * @param CssHtmlTree aTree, the owning CssHtmlTree
 * @param aSelectorInfo
 */
function SelectorView(aTree, aSelectorInfo)
{
  this.tree = aTree;
  this.selectorInfo = aSelectorInfo;
  this._cacheStatusNames();

  this.openStyleEditor = this.openStyleEditor.bind(this);
  this.maybeOpenStyleEditor = this.maybeOpenStyleEditor.bind(this);

  this.updateSourceLink();
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
   *
   * @return {void}
   */
  _cacheStatusNames: function SelectorView_cacheStatusNames()
  {
    if (SelectorView.STATUS_NAMES.length) {
      return;
    }

    for (let status in CssLogic.STATUS) {
      let i = CssLogic.STATUS[status];
      if (i > CssLogic.STATUS.UNMATCHED) {
        let value = CssHtmlTree.l10n("rule.status." + status);
        // Replace normal spaces with non-breaking spaces
        SelectorView.STATUS_NAMES[i] = value.replace(/ /g, '\u00A0');
      }
    }
  },

  /**
   * A localized version of cssRule.status
   */
  get statusText()
  {
    return SelectorView.STATUS_NAMES[this.selectorInfo.status];
  },

  /**
   * Get class name for selector depending on status
   */
  get statusClass()
  {
    return SelectorView.CLASS_NAMES[this.selectorInfo.status - 1];
  },

  get href()
  {
    if (this._href) {
      return this._href;
    }
    let sheet = this.selectorInfo.rule.parentStyleSheet;
    this._href = sheet ? sheet.href : "#";
    return this._href;
  },

  get sourceText()
  {
    return this.selectorInfo.sourceText;
  },


  get value()
  {
    return this.selectorInfo.value;
  },

  get outputFragment()
  {
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
    });
    return frag;
  },

  /**
   * Update the text of the source link to reflect whether we're showing
   * original sources or not.
   */
  updateSourceLink: function()
  {
    return this.updateSource().then((oldSource) => {
      if (oldSource != this.source && this.tree.element) {
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
  updateSource: function()
  {
    let rule = this.selectorInfo.rule;
    this.sheet = rule.parentStyleSheet;

    if (!rule || !this.sheet) {
      let oldSource = this.source;
      this.source = CssLogic.l10n("rule.sourceElement");
      this.href = "#";
      return promise.resolve(oldSource);
    }

    let showOrig = Services.prefs.getBoolPref(PREF_ORIG_SOURCES);

    if (showOrig && rule.type != ELEMENT_STYLE) {
      let deferred = promise.defer();

      // set as this first so we show something while we're fetching
      this.source = CssLogic.shortSource(this.sheet) + ":" + rule.line;

      rule.getOriginalLocation().then(({href, line, column}) => {
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
   * Open the style editor if the RETURN key was pressed.
   */
  maybeOpenStyleEditor: function(aEvent)
  {
    let keyEvent = Ci.nsIDOMKeyEvent;
    if (aEvent.keyCode == keyEvent.DOM_VK_RETURN) {
      this.openStyleEditor();
    }
  },

  /**
   * When a css link is clicked this method is called in order to either:
   *   1. Open the link in view source (for chrome stylesheets).
   *   2. Open the link in the style editor.
   *
   *   We can only view stylesheets contained in document.styleSheets inside the
   *   style editor.
   *
   * @param aEvent The click event
   */
  openStyleEditor: function(aEvent)
  {
    let inspector = this.tree.inspector;
    let rule = this.selectorInfo.rule;

    // The style editor can only display stylesheets coming from content because
    // chrome stylesheets are not listed in the editor's stylesheet selector.
    //
    // If the stylesheet is a content stylesheet we send it to the style
    // editor else we display it in the view source window.
    let sheet = rule.parentStyleSheet;
    if (!sheet || sheet.isSystem) {
      let contentDoc = null;
      if (this.tree.viewedElement.isLocal_toBeDeprecated()) {
        let rawNode = this.tree.viewedElement.rawNode();
        if (rawNode) {
          contentDoc = rawNode.ownerDocument;
        }
      }
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
        gDevTools.showToolbox(target, "styleeditor").then(function(toolbox) {
          let sheet = source || href;
          toolbox.getCurrentPanel().selectStyleSheet(sheet, line, column);
        });
      }
    });
  }
};

/**
 * Create a child element with a set of attributes.
 *
 * @param {Element} aParent
 *        The parent node.
 * @param {string} aTag
 *        The tag name.
 * @param {object} aAttributes
 *        A set of attributes to set on the node.
 */
function createChild(aParent, aTag, aAttributes={}) {
  let elt = aParent.ownerDocument.createElementNS(HTML_NS, aTag);
  for (let attr in aAttributes) {
    if (aAttributes.hasOwnProperty(attr)) {
      if (attr === "textContent") {
        elt.textContent = aAttributes[attr];
      } else if(attr === "child") {
        elt.appendChild(aAttributes[attr]);
      } else {
        elt.setAttribute(attr, aAttributes[attr]);
      }
    }
  }
  aParent.appendChild(elt);
  return elt;
}

exports.CssHtmlTree = CssHtmlTree;
exports.PropertyView = PropertyView;
