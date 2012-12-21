/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const Ci = Components.interfaces;
const Cc = Components.classes;
const Cu = Components.utils;
const FILTER_CHANGED_TIMEOUT = 300;

const HTML_NS = "http://www.w3.org/1999/xhtml";
const XUL_NS = "http://www.mozilla.org/keymaster/gatekeeper/there.is.only.xul";

Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource://gre/modules/PluralForm.jsm");
Cu.import("resource://gre/modules/XPCOMUtils.jsm");
Cu.import("resource:///modules/devtools/CssLogic.jsm");
Cu.import("resource:///modules/devtools/Templater.jsm");
Cu.import("resource:///modules/devtools/ToolDefinitions.jsm");

XPCOMUtils.defineLazyModuleGetter(this, "gDevTools",
                                  "resource:///modules/devtools/gDevTools.jsm");

this.EXPORTED_SYMBOLS = ["CssHtmlTree", "PropertyView"];

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
  this.iter = Iterator(aGenerator);
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
    if (this.cancelled) {
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
      throw e;
    }
  },

  _runBatch: function Y_runBatch()
  {
    let time = Date.now();
    while(!this.cancelled) {
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
 * @constructor
 */
this.CssHtmlTree = function CssHtmlTree(aStyleInspector)
{
  this.styleWindow = aStyleInspector.window;
  this.styleDocument = aStyleInspector.window.document;
  this.styleInspector = aStyleInspector;
  this.cssLogic = aStyleInspector.cssLogic;
  this.propertyViews = [];

  let chromeReg = Cc["@mozilla.org/chrome/chrome-registry;1"].
    getService(Ci.nsIXULChromeRegistry);
  this.getRTLAttr = chromeReg.isLocaleRTL("global") ? "rtl" : "ltr";

  // Create bound methods.
  this.siFocusWindow = this.focusWindow.bind(this);
  this.siBoundMenuUpdate = this.computedViewMenuUpdate.bind(this);
  this.siBoundCopy = this.computedViewCopy.bind(this);
  this.siBoundCopyDeclaration = this.computedViewCopyDeclaration.bind(this);
  this.siBoundCopyProperty = this.computedViewCopyProperty.bind(this);
  this.siBoundCopyPropertyValue = this.computedViewCopyPropertyValue.bind(this);

  this.styleDocument.addEventListener("copy", this.siBoundCopy);
  this.styleDocument.addEventListener("mousedown", this.siFocusWindow);

  // Nodes used in templating
  this.root = this.styleDocument.getElementById("root");
  this.templateRoot = this.styleDocument.getElementById("templateRoot");
  this.propertyContainer = this.styleDocument.getElementById("propertyContainer");
  this.panel = aStyleInspector.panel;

  // No results text.
  this.noResults = this.styleDocument.getElementById("noResults");

  // The element that we're inspecting, and the document that it comes from.
  this.viewedElement = null;
  this.createStyleViews();
  this.createContextMenu();
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
        .createBundle("chrome://browser/locale/devtools/styleinspector.properties"));

XPCOMUtils.defineLazyGetter(CssHtmlTree, "HELP_LINK_TITLE", function() {
  return CssHtmlTree.HELP_LINK_TITLE = CssHtmlTree.l10n("helpLinkTitle");
});

XPCOMUtils.defineLazyGetter(this, "clipboardHelper", function() {
  return Cc["@mozilla.org/widget/clipboardhelper;1"].
    getService(Ci.nsIClipboardHelper);
});

CssHtmlTree.prototype = {
  // Cache the list of properties that have matched and unmatched properties.
  _matchedProperties: null,
  _unmatchedProperties: null,

  htmlComplete: false,

  // Used for cancelling timeouts in the style filter.
  _filterChangedTimeout: null,

  // The search filter
  searchField: null,

  // Reference to the "Only user Styles" checkbox.
  onlyUserStylesCheckbox: null,

  // Holds the ID of the panelRefresh timeout.
  _panelRefreshTimeout: null,

  // Toggle for zebra striping
  _darkStripe: true,

  // Number of visible properties
  numVisibleProperties: 0,

  get showOnlyUserStyles()
  {
    return this.onlyUserStylesCheckbox.checked;
  },

  /**
   * Update the highlighted element. The CssHtmlTree panel will show the style
   * information for the given element.
   * @param {nsIDOMElement} aElement The highlighted node to get styles for.
   */
  highlight: function CssHtmlTree_highlight(aElement)
  {
    this.viewedElement = aElement;
    this._unmatchedProperties = null;
    this._matchedProperties = null;

    if (this.htmlComplete) {
      this.refreshSourceFilter();
      this.refreshPanel();
    } else {
      if (this._refreshProcess) {
        this._refreshProcess.cancel();
      }

      CssHtmlTree.processTemplate(this.templateRoot, this.root, this);

      // Refresh source filter ... this must be done after templateRoot has been
      // processed.
      this.refreshSourceFilter();
      this.numVisibleProperties = 0;
      let fragment = this.styleDocument.createDocumentFragment();
      this._refreshProcess = new UpdateProcess(this.styleWindow, CssHtmlTree.propertyNames, {
        onItem: function(aPropertyName) {
          // Per-item callback.
          let propView = new PropertyView(this, aPropertyName);
          fragment.appendChild(propView.buildMain());
          fragment.appendChild(propView.buildSelectorContainer());

          if (propView.visible) {
            this.numVisibleProperties++;
          }
          propView.refreshAllSelectors();
          this.propertyViews.push(propView);
        }.bind(this),
        onDone: function() {
          // Completed callback.
          this.htmlComplete = true;
          this.propertyContainer.appendChild(fragment);
          this.noResults.hidden = this.numVisibleProperties > 0;
          this._refreshProcess = null;

          // If a refresh was scheduled during the building, complete it.
          if (this._needsRefresh) {
            delete this._needsRefresh;
            this.refreshPanel();
          } else {
            Services.obs.notifyObservers(null, "StyleInspector-populated", null);
          }
        }.bind(this)});

      this._refreshProcess.schedule();
    }
  },

  /**
   * Refresh the panel content.
   */
  refreshPanel: function CssHtmlTree_refreshPanel()
  {
    // If we're still in the process of creating the initial layout,
    // leave it alone.
    if (!this.htmlComplete) {
      if (this._refreshProcess) {
        this._needsRefresh = true;
      }
      return;
    }

    if (this._refreshProcess) {
      this._refreshProcess.cancel();
    }

    this.noResults.hidden = true;

    // Reset visible property count
    this.numVisibleProperties = 0;

    // Reset zebra striping.
    this._darkStripe = true;

    let display = this.propertyContainer.style.display;
    this._refreshProcess = new UpdateProcess(this.styleWindow, this.propertyViews, {
      onItem: function(aPropView) {
        aPropView.refresh();
      }.bind(this),
      onDone: function() {
        this._refreshProcess = null;
        this.noResults.hidden = this.numVisibleProperties > 0;
        Services.obs.notifyObservers(null, "StyleInspector-populated", null);
      }.bind(this)
    });
    this._refreshProcess.schedule();
  },

  /**
   * Called when the user enters a search term.
   *
   * @param {Event} aEvent the DOM Event object.
   */
  filterChanged: function CssHtmlTree_filterChanged(aEvent)
  {
    let win = this.styleWindow;

    if (this._filterChangedTimeout) {
      win.clearTimeout(this._filterChangedTimeout);
    }

    this._filterChangedTimeout = win.setTimeout(function() {
      this.refreshPanel();
      this._filterChangeTimeout = null;
    }.bind(this), FILTER_CHANGED_TIMEOUT);
  },

  /**
   * The change event handler for the onlyUserStyles checkbox.
   *
   * @param {Event} aEvent the DOM Event object.
   */
  onlyUserStylesChanged: function CssHtmltree_onlyUserStylesChanged(aEvent)
  {
    this.refreshSourceFilter();
    this.refreshPanel();
  },

  /**
   * When onlyUserStyles.checked is true we only display properties that have
   * matched selectors and have been included by the document or one of the
   * document's stylesheets. If .checked is false we display all properties
   * including those that come from UA stylesheets.
   */
  refreshSourceFilter: function CssHtmlTree_setSourceFilter()
  {
    this._matchedProperties = null;
    this.cssLogic.sourceFilter = this.showOnlyUserStyles ?
                                 CssLogic.FILTER.ALL :
                                 CssLogic.FILTER.UA;
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
      if (prop.charAt(0) == "-") {
        mozProps.push(prop);
      } else {
        CssHtmlTree.propertyNames.push(prop);
      }
    }

    CssHtmlTree.propertyNames.sort();
    CssHtmlTree.propertyNames.push.apply(CssHtmlTree.propertyNames,
      mozProps.sort());
  },

  /**
   * Get a list of properties that have matched selectors.
   *
   * @return {object} the object maps property names (keys) to booleans (values)
   * that tell if the given property has matched selectors or not.
   */
  get matchedProperties()
  {
    if (!this._matchedProperties) {
      this._matchedProperties =
        this.cssLogic.hasMatchedSelectors(CssHtmlTree.propertyNames);
    }
    return this._matchedProperties;
  },

  /**
   * Check if a property has unmatched selectors. Result is cached.
   *
   * @param {string} aProperty the name of the property you want to check.
   * @return {boolean} true if the property has unmatched selectors, false
   * otherwise.
   */
  hasUnmatchedSelectors: function CssHtmlTree_hasUnmatchedSelectors(aProperty)
  {
    // Initially check all of the properties that return false for
    // hasMatchedSelectors(). This speeds-up the UI.
    if (!this._unmatchedProperties) {
      let properties = [];
      CssHtmlTree.propertyNames.forEach(function(aName) {
        if (!this.matchedProperties[aName]) {
          properties.push(aName);
        }
      }, this);

      if (properties.indexOf(aProperty) == -1) {
        properties.push(aProperty);
      }

      this._unmatchedProperties = this.cssLogic.hasUnmatchedSelectors(properties);
    }

    // Lazy-get the result for properties we do not have cached.
    if (!(aProperty in this._unmatchedProperties)) {
      let result = this.cssLogic.hasUnmatchedSelectors([aProperty]);
      this._unmatchedProperties[aProperty] = result[aProperty];
    }

    return this._unmatchedProperties[aProperty];
  },

  /**
   * Create a context menu.
   */
  createContextMenu: function SI_createContextMenu()
  {
    let iframe = this.styleInspector.outerIFrame;
    let outerDoc = iframe.ownerDocument;

    let popupSet = outerDoc.getElementById("inspectorPopupSet");

    let menu = outerDoc.createElement("menupopup");
    menu.addEventListener("popupshowing", this.siBoundMenuUpdate);
    menu.id = "computed-view-context-menu";
    popupSet.appendChild(menu);

    // Copy selection
    let label = CssHtmlTree.l10n("style.contextmenu.copyselection");
    let accessKey = CssHtmlTree.l10n("style.contextmenu.copyselection.accesskey");
    let item = outerDoc.createElement("menuitem");
    item.id = "computed-view-copy";
    item.setAttribute("label", label);
    item.setAttribute("accesskey", accessKey);
    item.addEventListener("command", this.siBoundCopy);
    menu.appendChild(item);

    // Copy declaration
    label = CssHtmlTree.l10n("style.contextmenu.copydeclaration");
    accessKey = CssHtmlTree.l10n("style.contextmenu.copydeclaration.accesskey");
    item = outerDoc.createElement("menuitem");
    item.id = "computed-view-copy-declaration";
    item.setAttribute("label", label);
    item.setAttribute("accesskey", accessKey);
    item.addEventListener("command", this.siBoundCopyDeclaration);
    menu.appendChild(item);

    // Copy property name
    label = CssHtmlTree.l10n("style.contextmenu.copyproperty");
    accessKey = CssHtmlTree.l10n("style.contextmenu.copyproperty.accesskey");
    item = outerDoc.createElement("menuitem");
    item.id = "computed-view-copy-property";
    item.setAttribute("label", label);
    item.setAttribute("accesskey", accessKey);
    item.addEventListener("command", this.siBoundCopyProperty);
    menu.appendChild(item);

    // Copy property value
    label = CssHtmlTree.l10n("style.contextmenu.copypropertyvalue");
    accessKey = CssHtmlTree.l10n("style.contextmenu.copypropertyvalue.accesskey");
    item = outerDoc.createElement("menuitem");
    item.id = "computed-view-copy-property-value";
    item.setAttribute("label", label);
    item.setAttribute("accesskey", accessKey);
    item.addEventListener("command", this.siBoundCopyPropertyValue);
    menu.appendChild(item);

    iframe.setAttribute("context", menu.id);
  },

  /**
   * Update the context menu by disabling irrelevant menuitems and enabling
   * relevant ones.
   */
  computedViewMenuUpdate: function si_computedViewMenuUpdate()
  {
    let disable = this.styleWindow.getSelection().isCollapsed;

    let outerDoc = this.styleInspector.outerIFrame.ownerDocument;
    let menuitem = outerDoc.querySelector("#computed-view-copy");
    menuitem.disabled = disable;

    let node = outerDoc.popupNode;
    if (!node) {
      return;
    }

    if (!node.classList.contains("property-view")) {
      while (node = node.parentElement) {
        if (node.classList.contains("property-view")) {
          break;
        }
      }
    }
    let disablePropertyItems = !node;
    menuitem = outerDoc.querySelector("#computed-view-copy-declaration");
    menuitem.disabled = disablePropertyItems;
    menuitem = outerDoc.querySelector("#computed-view-copy-property");
    menuitem.disabled = disablePropertyItems;
    menuitem = outerDoc.querySelector("#computed-view-copy-property-value");
    menuitem.disabled = disablePropertyItems;
  },

  /**
   * Focus the window on mousedown.
   *
   * @param aEvent The event object
   */
  focusWindow: function si_focusWindow(aEvent)
  {
    let win = this.styleDocument.defaultView;
    win.focus();
  },

  /**
   * Copy selected text.
   *
   * @param aEvent The event object
   */
  computedViewCopy: function si_computedViewCopy(aEvent)
  {
    let win = this.styleDocument.defaultView;
    let text = win.getSelection().toString();

    // Tidy up block headings by moving CSS property names and their values onto
    // the same line and inserting a colon between them.
    text = text.replace(/\t(.+)\t\t(.+)/g, "$1: $2");

    // Remove any MDN link titles
    text = text.replace(CssHtmlTree.HELP_LINK_TITLE, "");
    let outerDoc = this.styleInspector.outerIFrame.ownerDocument;
    clipboardHelper.copyString(text, outerDoc);

    if (aEvent) {
      aEvent.preventDefault();
    }
  },

  /**
   * Copy declaration.
   *
   * @param aEvent The event object
   */
  computedViewCopyDeclaration: function si_computedViewCopyDeclaration(aEvent)
  {
    let outerDoc = this.styleInspector.outerIFrame.ownerDocument;
    let node = outerDoc.popupNode;
    if (!node) {
      return;
    }

    if (!node.classList.contains("property-view")) {
      while (node = node.parentElement) {
        if (node.classList.contains("property-view")) {
          break;
        }
      }
    }
    if (node) {
      let name = node.querySelector(".property-name").textContent;
      let value = node.querySelector(".property-value").textContent;

      clipboardHelper.copyString(name + ": " + value + ";", outerDoc);
    }
  },

  /**
   * Copy property name.
   *
   * @param aEvent The event object
   */
  computedViewCopyProperty: function si_computedViewCopyProperty(aEvent)
  {
    let outerDoc = this.styleInspector.outerIFrame.ownerDocument;
    let node = outerDoc.popupNode;
    if (!node) {
      return;
    }

    if (!node.classList.contains("property-view")) {
      while (node = node.parentElement) {
        if (node.classList.contains("property-view")) {
          break;
        }
      }
    }
    if (node) {
      node = node.querySelector(".property-name");
      clipboardHelper.copyString(node.textContent, outerDoc);
    }
  },

  /**
   * Copy property value.
   *
   * @param aEvent The event object
   */
  computedViewCopyPropertyValue: function si_computedViewCopyPropertyValue(aEvent)
  {
    let outerDoc = this.styleInspector.outerIFrame.ownerDocument;
    let node = outerDoc.popupNode;
    if (!node) {
      return;
    }

    if (!node.classList.contains("property-view")) {
      while (node = node.parentElement) {
        if (node.classList.contains("property-view")) {
          break;
        }
      }
    }
    if (node) {
      node = node.querySelector(".property-value");
      clipboardHelper.copyString(node.textContent, outerDoc);
    }
  },

  /**
   * Destructor for CssHtmlTree.
   */
  destroy: function CssHtmlTree_destroy()
  {
    delete this.viewedElement;

    // Remove event listeners
    this.onlyUserStylesCheckbox.removeEventListener("command",
      this.onlyUserStylesChanged);
    this.searchField.removeEventListener("command", this.filterChanged);

    // Cancel tree construction
    if (this._refreshProcess) {
      this._refreshProcess.cancel();
    }

    // Remove context menu
    let outerDoc = this.styleInspector.outerIFrame.ownerDocument;
    let menu = outerDoc.querySelector("#computed-view-context-menu");
    if (menu) {
      // Copy selected
      let menuitem = outerDoc.querySelector("#computed-view-copy");
      menuitem.removeEventListener("command", this.siBoundCopy);

      // Copy property
      menuitem = outerDoc.querySelector("#computed-view-copy-declaration");
      menuitem.removeEventListener("command", this.siBoundCopyDeclaration);

      // Copy property name
      menuitem = outerDoc.querySelector("#computed-view-copy-property");
      menuitem.removeEventListener("command", this.siBoundCopyProperty);

      // Copy property value
      menuitem = outerDoc.querySelector("#computed-view-copy-property-value");
      menuitem.removeEventListener("command", this.siBoundCopyPropertyValue);

      menu.removeEventListener("popupshowing", this.siBoundMenuUpdate);
      menu.parentNode.removeChild(menu);
    }

    // Remove bound listeners
    this.styleDocument.removeEventListener("copy", this.siBoundCopy);
    this.styleDocument.removeEventListener("mousedown", this.siFocusWindow);

    // Nodes used in templating
    delete this.root;
    delete this.propertyContainer;
    delete this.panel;

    // The document in which we display the results (csshtmltree.xul).
    delete this.styleDocument;

    // The element that we're inspecting, and the document that it comes from.
    delete this.propertyViews;
    delete this.styleWindow;
    delete this.styleDocument;
    delete this.cssLogic;
    delete this.styleInspector;
  },
};

/**
 * A container to give easy access to property data from the template engine.
 *
 * @constructor
 * @param {CssHtmlTree} aTree the CssHtmlTree instance we are working with.
 * @param {string} aName the CSS property name for which this PropertyView
 * instance will render the rules.
 */
this.PropertyView = function PropertyView(aTree, aName)
{
  this.tree = aTree;
  this.name = aName;
  this.getRTLAttr = aTree.getRTLAttr;

  this.link = "https://developer.mozilla.org/CSS/" + aName;

  this.templateMatchedSelectors = aTree.styleDocument.getElementById("templateMatchedSelectors");
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

  // Are unmatched rules expanded?
  unmatchedExpanded: false,

  // Unmatched selector table
  unmatchedSelectorTable: null,

  // Matched selector container
  matchedSelectorsContainer: null,

  // Matched selector expando
  matchedExpander: null,

  // Unmatched selector expando
  unmatchedExpander: null,

  // Unmatched selector container
  unmatchedSelectorsContainer: null,

  // Unmatched title block
  unmatchedTitleBlock: null,

  // Cache for matched selector views
  _matchedSelectorViews: null,

  // Cache for unmatched selector views
  _unmatchedSelectorViews: null,

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
    return this.tree.cssLogic.getPropertyInfo(this.name);
  },

  /**
   * Does the property have any matched selectors?
   */
  get hasMatchedSelectors()
  {
    return this.name in this.tree.matchedProperties;
  },

  /**
   * Does the property have any unmatched selectors?
   */
  get hasUnmatchedSelectors()
  {
    return this.name in this.tree.hasUnmatchedSelectors;
  },

  /**
   * Should this property be visible?
   */
  get visible()
  {
    if (this.tree.showOnlyUserStyles && !this.hasMatchedSelectors) {
      return false;
    }

    let searchTerm = this.tree.searchField.value.toLowerCase();
    if (searchTerm && this.name.toLowerCase().indexOf(searchTerm) == -1 &&
      this.value.toLowerCase().indexOf(searchTerm) == -1) {
      return false;
    }

    return true;
  },

  /**
   * Returns the className that should be assigned to the propertyView.
   *
   * @return string
   */
  get propertyHeaderClassName()
  {
    if (this.visible) {
      this.tree._darkStripe = !this.tree._darkStripe;
      let darkValue = this.tree._darkStripe ?
                      "property-view darkrow" : "property-view";
      return darkValue;
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
      let darkValue = this.tree._darkStripe ?
                      "property-content darkrow" : "property-content";
      return darkValue;
    }
    return "property-content-hidden";
  },

  buildMain: function PropertyView_buildMain()
  {
    let doc = this.tree.styleDocument;
    this.element = doc.createElementNS(HTML_NS, "tr");
    this.element.setAttribute("class", this.propertyHeaderClassName);

    this.expanderContainer = doc.createElementNS(HTML_NS, "td");
    this.element.appendChild(this.expanderContainer);
    this.expanderContainer.setAttribute("class", "expander-container");

    this.matchedExpander = doc.createElementNS(HTML_NS, "div");
    this.matchedExpander.setAttribute("class", "match expander");
    this.matchedExpander.setAttribute("tabindex", "0");
    this.matchedExpander.addEventListener("click",
      this.matchedExpanderClick.bind(this), false);
    this.matchedExpander.addEventListener("keydown", function(aEvent) {
      let keyEvent = Ci.nsIDOMKeyEvent;
      if (aEvent.keyCode == keyEvent.DOM_VK_F1) {
        this.mdnLinkClick();
      }
      if (aEvent.keyCode == keyEvent.DOM_VK_RETURN ||
        aEvent.keyCode == keyEvent.DOM_VK_SPACE) {
        this.matchedExpanderClick(aEvent);
      }
    }.bind(this), false);
    this.expanderContainer.appendChild(this.matchedExpander);

    this.nameNode = doc.createElementNS(HTML_NS, "td");
    this.element.appendChild(this.nameNode);
    this.nameNode.setAttribute("class", "property-name");
    this.nameNode.textContent = this.name;
    this.nameNode.addEventListener("click", function(aEvent) {
      this.matchedExpander.focus();
    }.bind(this), false);

    let helpcontainer = doc.createElementNS(HTML_NS, "td");
    this.element.appendChild(helpcontainer);
    helpcontainer.setAttribute("class", "helplink-container");

    let helplink = doc.createElementNS(HTML_NS, "a");
    helpcontainer.appendChild(helplink);
    helplink.setAttribute("class", "helplink");
    helplink.setAttribute("title", CssHtmlTree.HELP_LINK_TITLE);
    helplink.textContent = CssHtmlTree.HELP_LINK_TITLE;
    helplink.addEventListener("click", this.mdnLinkClick.bind(this), false);

    this.valueNode = doc.createElementNS(HTML_NS, "td");
    this.element.appendChild(this.valueNode);
    this.valueNode.setAttribute("class", "property-value");
    this.valueNode.setAttribute("dir", "ltr");
    this.valueNode.textContent = this.value;

    return this.element;
  },

  buildSelectorContainer: function PropertyView_buildSelectorContainer()
  {
    let doc = this.tree.styleDocument;
    let element = doc.createElementNS(HTML_NS, "tr");
    element.setAttribute("class", this.propertyContentClassName);
    this.matchedSelectorsContainer = doc.createElementNS(HTML_NS, "td");
    this.matchedSelectorsContainer.setAttribute("colspan", "0");
    this.matchedSelectorsContainer.setAttribute("class", "rulelink");
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
      this._unmatchedSelectorViews = null;
      this.prevViewedElement = this.tree.viewedElement;
    }

    if (!this.tree.viewedElement || !this.visible) {
      this.valueNode.textContent = "";
      this.matchedSelectorsContainer.parentNode.hidden = true;
      this.matchedSelectorsContainer.textContent = "";
      this.matchedExpander.removeAttribute("open");
      return;
    }

    this.tree.numVisibleProperties++;
    this.valueNode.textContent = this.propertyInfo.value;
    this.refreshAllSelectors();
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
      CssHtmlTree.processTemplate(this.templateMatchedSelectors,
        this.matchedSelectorsContainer, this);
      this.matchedExpander.setAttribute("open", "");
    } else {
      this.matchedSelectorsContainer.innerHTML = "";
      this.matchedExpander.removeAttribute("open");
    }
  },

  /**
   * Refresh the panel unmatched rules.
   */
  refreshUnmatchedSelectors: function PropertyView_refreshUnmatchedSelectors()
  {
    let hasMatchedSelectors = this.hasMatchedSelectors;

    this.unmatchedSelectorTable.hidden = !this.unmatchedExpanded;

    if (hasMatchedSelectors) {
      this.unmatchedSelectorsContainer.hidden = !this.matchedExpanded ||
        !this.hasUnmatchedSelectors;
      this.unmatchedTitleBlock.hidden = false;
    } else {
      this.unmatchedSelectorsContainer.hidden = !this.unmatchedExpanded;
      this.unmatchedTitleBlock.hidden = true;
    }

    if (this.unmatchedExpanded && this.hasUnmatchedSelectors) {
      CssHtmlTree.processTemplate(this.templateUnmatchedSelectors,
        this.unmatchedSelectorTable, this);
      if (!hasMatchedSelectors) {
        this.matchedExpander.setAttribute("open", "");
        this.unmatchedSelectorTable.classList.add("only-unmatched");
      } else {
        this.unmatchedExpander.setAttribute("open", "");
        this.unmatchedSelectorTable.classList.remove("only-unmatched");
      }
    } else {
      if (!hasMatchedSelectors) {
        this.matchedExpander.removeAttribute("open");
      }
      this.unmatchedExpander.removeAttribute("open");
      this.unmatchedSelectorTable.innerHTML = "";
    }
  },

  /**
   * Refresh the panel matched and unmatched rules
   */
  refreshAllSelectors: function PropertyView_refreshAllSelectors()
  {
    this.refreshMatchedSelectors();
  },

  /**
   * Provide access to the matched SelectorViews that we are currently
   * displaying.
   */
  get matchedSelectorViews()
  {
    if (!this._matchedSelectorViews) {
      this._matchedSelectorViews = [];
      this.propertyInfo.matchedSelectors.forEach(
        function matchedSelectorViews_convert(aSelectorInfo) {
          this._matchedSelectorViews.push(new SelectorView(this.tree, aSelectorInfo));
        }, this);
    }

    return this._matchedSelectorViews;
  },

    /**
   * Provide access to the unmatched SelectorViews that we are currently
   * displaying.
   */
  get unmatchedSelectorViews()
  {
    if (!this._unmatchedSelectorViews) {
      this._unmatchedSelectorViews = [];
      this.propertyInfo.unmatchedSelectors.forEach(
        function unmatchedSelectorViews_convert(aSelectorInfo) {
          this._unmatchedSelectorViews.push(new SelectorView(this.tree, aSelectorInfo));
        }, this);
    }

    return this._unmatchedSelectorViews;
  },

  /**
   * The action when a user expands matched selectors.
   *
   * @param {Event} aEvent Used to determine the class name of the targets click
   * event.
   */
  matchedExpanderClick: function PropertyView_matchedExpanderClick(aEvent)
  {
    this.matchedExpanded = !this.matchedExpanded;
    this.refreshAllSelectors();
    aEvent.preventDefault();
  },

  /**
   * The action when a user expands unmatched selectors.
   */
  unmatchedSelectorsClick: function PropertyView_unmatchedSelectorsClick(aEvent)
  {
    this.unmatchedExpanded = !this.unmatchedExpanded;
    this.refreshUnmatchedSelectors();
    aEvent.preventDefault();
  },

  /**
   * The action when a user clicks on the MDN help link for a property.
   */
  mdnLinkClick: function PropertyView_mdnLinkClick(aEvent)
  {
    let inspector = this.tree.styleInspector.inspector;

    if (inspector.target.tab) {
      let browserWin = inspector.target.tab.ownerDocument.defaultView;
      browserWin.openUILinkIn(this.link, "tab");
    }
    aEvent.preventDefault();
  },
};

/**
 * A container to view us easy access to display data from a CssRule
 * @param CssHtmlTree aTree, the owning CssHtmlTree
 * @param aSelectorInfo
 */
function SelectorView(aTree, aSelectorInfo)
{
  this.tree = aTree;
  this.selectorInfo = aSelectorInfo;
  this._cacheStatusNames();
}

/**
 * Decode for cssInfo.rule.status
 * @see SelectorView.prototype._cacheStatusNames
 * @see CssLogic.STATUS
 */
SelectorView.STATUS_NAMES = [
  // "Unmatched", "Parent Match", "Matched", "Best Match"
];

SelectorView.CLASS_NAMES = [
  "unmatched", "parentmatch", "matched", "bestmatch"
];

SelectorView.prototype = {
  /**
   * Cache localized status names.
   *
   * These statuses are localized inside the styleinspector.properties string
   * bundle.
   * @see CssLogic.jsm - the CssLogic.STATUS array.
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
      if (i > -1) {
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
    return SelectorView.CLASS_NAMES[this.selectorInfo.status];
  },

  /**
   * A localized Get localized human readable info
   */
  humanReadableText: function SelectorView_humanReadableText(aElement)
  {
    if (this.tree.getRTLAttr == "rtl") {
      return this.selectorInfo.value + " \u2190 " + this.text(aElement);
    } else {
      return this.text(aElement) + " \u2192 " + this.selectorInfo.value;
    }
  },

  text: function SelectorView_text(aElement) {
    let result = this.selectorInfo.selector.text;
    if (this.selectorInfo.elementStyle) {
      let source = this.selectorInfo.sourceElement;
      let IUI = this.tree.styleInspector.IUI;
      if (IUI && IUI.selection == source) {
        result = "this";
      } else {
        result = CssLogic.getShortName(source);
      }

      result += ".style";
    }

    return result;
  },

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
    let inspector = this.tree.styleInspector.inspector;
    let contentDoc = inspector.selection.document;
    let cssSheet = this.selectorInfo.selector._cssRule._cssSheet;
    let line = this.selectorInfo.ruleLine || 0;
    let contentSheet = false;
    let styleSheet;
    let styleSheets;

    // The style editor can only display stylesheets coming from content because
    // chrome stylesheets are not listed in the editor's stylesheet selector.
    //
    // If the stylesheet is a content stylesheet we send it to the style
    // editor else we display it in the view source window.
    //
    // We check if cssSheet exists in case of inline styles (which contain no
    // sheet)
    if (cssSheet) {
      styleSheet = cssSheet.domSheet;
      styleSheets = contentDoc.styleSheets;

      // Array.prototype.indexOf always returns -1 here so we loop through
      // the styleSheets array instead.
      for each (let sheet in styleSheets) {
        if (sheet == styleSheet) {
          contentSheet = true;
          break;
        }
      }
    }

    if (contentSheet) {
      let target = inspector.target;

      if (styleEditorDefinition.isTargetSupported(target)) {
        gDevTools.showToolbox(target, "styleeditor").then(function(toolbox) {
          toolbox.getCurrentPanel().selectStyleSheet(styleSheet, line);
        });
      }
    } else {
      let href = styleSheet ? styleSheet.href : "";
      let viewSourceUtils = inspector.viewSourceUtils;

      if (this.selectorInfo.sourceElement) {
        href = this.selectorInfo.sourceElement.ownerDocument.location.href;
      }
      viewSourceUtils.viewSource(href, null, contentDoc, line);
    }
  },
};
