/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

this.EXPORTED_SYMBOLS = ["CustomizableUI"];

const {classes: Cc, interfaces: Ci, utils: Cu, results: Cr} = Components;

Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource://gre/modules/XPCOMUtils.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "PanelWideWidgetTracker",
  "resource:///modules/PanelWideWidgetTracker.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "CustomizableWidgets",
  "resource:///modules/CustomizableWidgets.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "DeferredTask",
  "resource://gre/modules/DeferredTask.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "PrivateBrowsingUtils",
  "resource://gre/modules/PrivateBrowsingUtils.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "Promise",
  "resource://gre/modules/Promise.jsm");
XPCOMUtils.defineLazyGetter(this, "gWidgetsBundle", function() {
  const kUrl = "chrome://browser/locale/customizableui/customizableWidgets.properties";
  return Services.strings.createBundle(kUrl);
});
XPCOMUtils.defineLazyModuleGetter(this, "ShortcutUtils",
  "resource://gre/modules/ShortcutUtils.jsm");
XPCOMUtils.defineLazyServiceGetter(this, "gELS",
  "@mozilla.org/eventlistenerservice;1", "nsIEventListenerService");

const kNSXUL = "http://www.mozilla.org/keymaster/gatekeeper/there.is.only.xul";

const kSpecialWidgetPfx = "customizableui-special-";

const kPrefCustomizationState        = "browser.uiCustomization.state";
const kPrefCustomizationAutoAdd      = "browser.uiCustomization.autoAdd";
const kPrefCustomizationDebug        = "browser.uiCustomization.debug";

/**
 * The keys are the handlers that are fired when the event type (the value)
 * is fired on the subview. A widget that provides a subview has the option
 * of providing onViewShowing and onViewHiding event handlers.
 */
const kSubviewEvents = [
  "ViewShowing",
  "ViewHiding"
];

/**
 * gPalette is a map of every widget that CustomizableUI.jsm knows about, keyed
 * on their IDs.
 */
let gPalette = new Map();

/**
 * gAreas maps area IDs to Sets of properties about those areas. An area is a
 * place where a widget can be put.
 */
let gAreas = new Map();

/**
 * gPlacements maps area IDs to Arrays of widget IDs, indicating that the widgets
 * are placed within that area (either directly in the area node, or in the
 * customizationTarget of the node).
 */
let gPlacements = new Map();

/**
 * gFuturePlacements represent placements that will happen for areas that have
 * not yet loaded (due to lazy-loading). This can occur when add-ons register
 * widgets.
 */
let gFuturePlacements = new Map();

//XXXunf Temporary. Need a nice way to abstract functions to build widgets
//       of these types.
let gSupportedWidgetTypes = new Set(["button", "view", "custom"]);

/**
 * gPanelsForWindow is a list of known panels in a window which we may need to close
 * should command events fire which target them.
 */
let gPanelsForWindow = new WeakMap();

/**
 * gSeenWidgets remembers which widgets the user has seen for the first time
 * before. This way, if a new widget is created, and the user has not seen it
 * before, it can be put in its default location. Otherwise, it remains in the
 * palette.
 */
let gSeenWidgets = new Set();

/**
 * gDirtyAreaCache is a set of area IDs for areas where items have been added,
 * moved or removed at least once. This set is persisted, and is used to
 * optimize building of toolbars in the default case where no toolbars should
 * be "dirty".
 */
let gDirtyAreaCache = new Set();

/**
 * gPendingBuildAreas is a map from area IDs to map from build nodes to their
 * existing children at the time of node registration, that are waiting
 * for the area to be registered
 */
let gPendingBuildAreas = new Map();

let gSavedState = null;
let gRestoring = false;
let gDirty = false;
let gInBatchStack = 0;
let gResetting = false;

/**
 * gBuildAreas maps area IDs to actual area nodes within browser windows.
 */
let gBuildAreas = new Map();

/**
 * gBuildWindows is a map of windows that have registered build areas, mapped
 * to a Set of known toolboxes in that window.
 */
let gBuildWindows = new Map();

let gNewElementCount = 0;
let gGroupWrapperCache = new Map();
let gSingleWrapperCache = new WeakMap();
let gListeners = new Set();

let gModuleName = "[CustomizableUI]";
#include logging.js

let CustomizableUIInternal = {
  initialize: function() {
    LOG("Initializing");

    this.addListener(this);
    this._defineBuiltInWidgets();
    this.loadSavedState();

    let panelPlacements = [
      "edit-controls",
      "zoom-controls",
      "new-window-button",
      "privatebrowsing-button",
      "save-page-button",
      "print-button",
      "history-panelmenu",
      "fullscreen-button",
      "find-button",
      "preferences-button",
      "add-ons-button",
      "developer-button",
    ];

    if (gPalette.has("switch-to-metro-button")) {
      panelPlacements.push("switch-to-metro-button");
    }

    let showCharacterEncoding = Services.prefs.getComplexValue(
      "browser.menu.showCharacterEncoding",
      Ci.nsIPrefLocalizedString
    ).data;
    if (showCharacterEncoding == "true") {
      panelPlacements.push("characterencoding-button");
    }

    this.registerArea(CustomizableUI.AREA_PANEL, {
      anchor: "PanelUI-menu-button",
      type: CustomizableUI.TYPE_MENU_PANEL,
      defaultPlacements: panelPlacements
    }, true);
    PanelWideWidgetTracker.init();

    this.registerArea(CustomizableUI.AREA_NAVBAR, {
      legacy: true,
      type: CustomizableUI.TYPE_TOOLBAR,
      overflowable: true,
      defaultPlacements: [
        "urlbar-container",
        "search-container",
        "webrtc-status-button",
        "bookmarks-menu-button",
        "downloads-button",
        "home-button",
        "social-share-button",
      ],
      defaultCollapsed: false,
    }, true);
#ifndef XP_MACOSX
    this.registerArea(CustomizableUI.AREA_MENUBAR, {
      legacy: true,
      type: CustomizableUI.TYPE_TOOLBAR,
      defaultPlacements: [
        "menubar-items",
      ],
      get defaultCollapsed() {
#ifdef MENUBAR_CAN_AUTOHIDE
#if defined(MOZ_WIDGET_GTK) || defined(MOZ_WIDGET_QT)
        return true;
#else
        // This is duplicated logic from /browser/base/jar.mn
        // for win6BrowserOverlay.xul.
        return Services.appinfo.OS == "WINNT" &&
               Services.sysinfo.getProperty("version") != "5.1";
#endif
#endif
        return false;
      }
    }, true);
#endif
    this.registerArea(CustomizableUI.AREA_TABSTRIP, {
      legacy: true,
      type: CustomizableUI.TYPE_TOOLBAR,
      defaultPlacements: [
        "tabbrowser-tabs",
        "new-tab-button",
        "alltabs-button",
        "tabs-closebutton",
      ],
      defaultCollapsed: false,
    }, true);
    this.registerArea(CustomizableUI.AREA_BOOKMARKS, {
      legacy: true,
      type: CustomizableUI.TYPE_TOOLBAR,
      defaultPlacements: [
        "personal-bookmarks",
      ],
      defaultCollapsed: true,
    }, true);

    this.registerArea(CustomizableUI.AREA_ADDONBAR, {
      type: CustomizableUI.TYPE_TOOLBAR,
      legacy: true,
      defaultPlacements: ["addonbar-closebutton", "status-bar"],
      defaultCollapsed: false,
    }, true);
  },

  _defineBuiltInWidgets: function() {
    //XXXunf Need to figure out how to auto-add new builtin widgets in new
    //       app versions to already customized areas.
    for (let widgetDefinition of CustomizableWidgets) {
      this.createBuiltinWidget(widgetDefinition);
    }
  },

  wrapWidget: function(aWidgetId) {
    if (gGroupWrapperCache.has(aWidgetId)) {
      return gGroupWrapperCache.get(aWidgetId);
    }

    let provider = this.getWidgetProvider(aWidgetId);
    if (!provider) {
      return null;
    }

    if (provider == CustomizableUI.PROVIDER_API) {
      let widget = gPalette.get(aWidgetId);
      if (!widget.wrapper) {
        widget.wrapper = new WidgetGroupWrapper(widget);
        gGroupWrapperCache.set(aWidgetId, widget.wrapper);
      }
      return widget.wrapper;
    }

    // PROVIDER_SPECIAL gets treated the same as PROVIDER_XUL.
    let wrapper = new XULWidgetGroupWrapper(aWidgetId);
    gGroupWrapperCache.set(aWidgetId, wrapper);
    return wrapper;
  },

  registerArea: function(aName, aProperties, aInternalCaller) {
    if (typeof aName != "string" || !/^[a-z0-9-_]{1,}$/i.test(aName)) {
      throw new Error("Invalid area name");
    }

    let areaIsKnown = gAreas.has(aName);
    let props = areaIsKnown ? gAreas.get(aName) : new Map();
    for (let key in aProperties) {
      //XXXgijs for special items, we need to make sure they have an appropriate ID
      // so we aren't perpetually in a non-default state:
      if (key == "defaultPlacements" && Array.isArray(aProperties[key])) {
        props.set(key, aProperties[key].map(x => this.isSpecialWidget(x) ? this.ensureSpecialWidgetId(x) : x ));
      } else {
        props.set(key, aProperties[key]);
      }
    }
    // Default to a toolbar:
    if (!props.has("type")) {
      props.set("type", CustomizableUI.TYPE_TOOLBAR);
    }
    if (props.get("type") == CustomizableUI.TYPE_TOOLBAR) {
      if (!aInternalCaller && props.has("defaultCollapsed")) {
        throw new Error("defaultCollapsed is only allowed for default toolbars.")
      }
      if (!props.has("defaultCollapsed")) {
        props.set("defaultCollapsed", true);
      }
    } else if (props.has("defaultCollapsed")) {
      throw new Error("defaultCollapsed only applies for TYPE_TOOLBAR areas.");
    }
    // Sanity check type:
    let allTypes = [CustomizableUI.TYPE_TOOLBAR, CustomizableUI.TYPE_MENU_PANEL];
    if (allTypes.indexOf(props.get("type")) == -1) {
      throw new Error("Invalid area type " + props.get("type"));
    }

    // And to no placements:
    if (!props.has("defaultPlacements")) {
      props.set("defaultPlacements", []);
    }
    // Sanity check default placements array:
    if (!Array.isArray(props.get("defaultPlacements"))) {
      throw new Error("Should provide an array of default placements");
    }

    if (!areaIsKnown) {
      gAreas.set(aName, props);

      if (props.get("legacy")) {
        // Guarantee this area exists in gFuturePlacements, to avoid checking it in
        // various places elsewhere.
        gFuturePlacements.set(aName, new Set());
      } else {
        this.restoreStateForArea(aName);
      }

      // If we have pending build area nodes, register all of them
      if (gPendingBuildAreas.has(aName)) {
        let pendingNodes = gPendingBuildAreas.get(aName);
        for (let [pendingNode, existingChildren] of pendingNodes) {
          this.registerToolbarNode(pendingNode, existingChildren);
        }
        gPendingBuildAreas.delete(aName);
      }
    }
  },

  unregisterArea: function(aName, aDestroyPlacements) {
    if (typeof aName != "string" || !/^[a-z0-9-_]{1,}$/i.test(aName)) {
      throw new Error("Invalid area name");
    }
    if (!gAreas.has(aName) && !gPlacements.has(aName)) {
      throw new Error("Area not registered");
    }

    // Move all the widgets out
    this.beginBatchUpdate();
    try {
      let placements = gPlacements.get(aName);
      if (placements) {
        // Need to clone this array so removeWidgetFromArea doesn't modify it
        placements = [...placements];
        placements.forEach(this.removeWidgetFromArea, this);
      }

      // Delete all remaining traces.
      gAreas.delete(aName);
      // Only destroy placements when necessary:
      if (aDestroyPlacements) {
        gPlacements.delete(aName);
      } else {
        // Otherwise we need to re-set them, as removeFromArea will have emptied
        // them out:
        gPlacements.set(aName, placements);
      }
      gFuturePlacements.delete(aName);
      gBuildAreas.delete(aName);
    } finally {
      this.endBatchUpdate(true);
    }
  },

  registerToolbarNode: function(aToolbar, aExistingChildren) {
    let area = aToolbar.id;
    if (gBuildAreas.has(area) && gBuildAreas.get(area).has(aToolbar)) {
      return;
    }
    let document = aToolbar.ownerDocument;
    let areaProperties = gAreas.get(area);

    // If this area is not registered, try to do it automatically:
    if (!areaProperties) {
      // If there's no default set attribute at all, we assume that we should
      // wait for registerArea to be called:
      if (!aToolbar.hasAttribute("defaultset")) {
        if (!gPendingBuildAreas.has(area)) {
          gPendingBuildAreas.set(area, new Map());
        }
        let pendingNodes = gPendingBuildAreas.get(area);
        pendingNodes.set(aToolbar, aExistingChildren);
        return;
      }
      let props = {type: CustomizableUI.TYPE_TOOLBAR, legacy: true};
      let defaultsetAttribute = aToolbar.getAttribute("defaultset");
      props.defaultPlacements = defaultsetAttribute.split(',').filter(s => s);
      this.registerArea(area, props);
      areaProperties = gAreas.get(area);
    }

    this.beginBatchUpdate();
    try {
      let placements = gPlacements.get(area);
      if (!placements && areaProperties.has("legacy")) {
        let legacyState = aToolbar.getAttribute("currentset");
        if (legacyState) {
          legacyState = legacyState.split(",").filter(s => s);
        }

        // Manually restore the state here, so the legacy state can be converted. 
        this.restoreStateForArea(area, legacyState);
        placements = gPlacements.get(area);
      }

      // Check that the current children and the current placements match. If
      // not, mark it as dirty:
      if (aExistingChildren.length != placements.length ||
          aExistingChildren.every((id, i) => id == placements[i])) {
        gDirtyAreaCache.add(area);
      }

      if (areaProperties.has("overflowable")) {
        aToolbar.overflowable = new OverflowableToolbar(aToolbar);
      }

      this.registerBuildArea(area, aToolbar);

      // We only build the toolbar if it's been marked as "dirty". Dirty means
      // one of the following things:
      // 1) Items have been added, moved or removed from this toolbar before.
      // 2) The number of children of the toolbar does not match the length of
      //    the placements array for that area.
      //
      // This notion of being "dirty" is stored in a cache which is persisted
      // in the saved state.
      if (gDirtyAreaCache.has(area)) {
        this.buildArea(area, placements, aToolbar);
      }
      aToolbar.setAttribute("currentset", placements.join(","));
    } finally {
      this.endBatchUpdate();
    }
  },

  buildArea: function(aArea, aPlacements, aAreaNode) {
    let document = aAreaNode.ownerDocument;
    let window = document.defaultView;
    let inPrivateWindow = PrivateBrowsingUtils.isWindowPrivate(window);
    let container = aAreaNode.customizationTarget;

    if (!container) {
      throw new Error("Expected area " + aArea
                      + " to have a customizationTarget attribute.");
    }

    // Restore nav-bar visibility since it may have been hidden
    // through a migration path (bug 938980) or an add-on.
    if (aArea == CustomizableUI.AREA_NAVBAR) {
      aAreaNode.collapsed = false;
    }

    this.beginBatchUpdate();

    try {
      let currentNode = container.firstChild;
      let placementsToRemove = new Set();
      for (let id of aPlacements) {
        while (currentNode && currentNode.getAttribute("skipintoolbarset") == "true") {
          currentNode = currentNode.nextSibling;
        }

        if (currentNode && currentNode.id == id) {
          currentNode = currentNode.nextSibling;
          continue;
        }

        let [provider, node] = this.getWidgetNode(id, window);
        if (!node) {
          LOG("Unknown widget: " + id);
          continue;
        }

        // If the placements have items in them which are (now) no longer removable,
        // we shouldn't be moving them:
        if (provider == CustomizableUI.PROVIDER_API) {
          let widgetInfo = gPalette.get(id);
          if (!widgetInfo.removable && aArea != widgetInfo.defaultArea) {
            placementsToRemove.add(id);
            continue;
          }
        } else if (provider == CustomizableUI.PROVIDER_XUL &&
                   node.parentNode != container && !this.isWidgetRemovable(node)) {
          placementsToRemove.add(id);
          continue;
        } // Special widgets are always removable, so no need to check them

        if (inPrivateWindow && provider == CustomizableUI.PROVIDER_API) {
          let widget = gPalette.get(id);
          if (!widget.showInPrivateBrowsing && inPrivateWindow) {
            continue;
          }
        }

        this.ensureButtonContextMenu(node, aAreaNode);
        if (node.localName == "toolbarbutton") {
          if (aArea == CustomizableUI.AREA_PANEL) {
            node.setAttribute("wrap", "true");
          } else {
            node.removeAttribute("wrap");
          }
        }

        this.insertWidgetBefore(node, currentNode, container, aArea);
        if (gResetting) {
          this.notifyListeners("onWidgetReset", node, container);
        }
      }

      if (currentNode) {
        let palette = aAreaNode.toolbox ? aAreaNode.toolbox.palette : null;
        let limit = currentNode.previousSibling;
        let node = container.lastChild;
        while (node && node != limit) {
          let previousSibling = node.previousSibling;
          // Nodes opt-in to removability. If they're removable, and we haven't
          // seen them in the placements array, then we toss them into the palette
          // if one exists. If no palette exists, we just remove the node. If the
          // node is not removable, we leave it where it is. However, we can only
          // safely touch elements that have an ID - both because we depend on
          // IDs, and because such elements are not intended to be widgets
          // (eg, titlebar-placeholder elements).
          if (node.id && node.getAttribute("skipintoolbarset") != "true") {
            if (this.isWidgetRemovable(node)) {
              if (palette && !this.isSpecialWidget(node.id)) {
                palette.appendChild(node);
                this.removeLocationAttributes(node);
              } else {
                container.removeChild(node);
              }
            } else {
              this.setLocationAttributes(currentNode, aArea);
              node.setAttribute("removable", false);
              LOG("Adding non-removable widget to placements of " + aArea + ": " +
                  node.id);
              gPlacements.get(aArea).push(node.id);
              gDirty = true;
            }
          }
          node = previousSibling;
        }
      }

      // If there are placements in here which aren't removable from their original area,
      // we remove them from this area's placement array. They will (have) be(en) added
      // to their original area's placements array in the block above this one.
      if (placementsToRemove.size) {
        let placementAry = gPlacements.get(aArea);
        for (let id of placementsToRemove) {
          let index = placementAry.indexOf(id);
          placementAry.splice(index, 1);
        }
      }

      if (gResetting) {
        this.notifyListeners("onAreaReset", aArea, container);
      }
    } finally {
      this.endBatchUpdate();
    }
  },

  addPanelCloseListeners: function(aPanel) {
    gELS.addSystemEventListener(aPanel, "click", this, false);
    gELS.addSystemEventListener(aPanel, "keypress", this, false);
    let win = aPanel.ownerDocument.defaultView;
    if (!gPanelsForWindow.has(win)) {
      gPanelsForWindow.set(win, new Set());
    }
    gPanelsForWindow.get(win).add(this._getPanelForNode(aPanel));
  },

  removePanelCloseListeners: function(aPanel) {
    gELS.removeSystemEventListener(aPanel, "click", this, false);
    gELS.removeSystemEventListener(aPanel, "keypress", this, false);
    let win = aPanel.ownerDocument.defaultView;
    let panels = gPanelsForWindow.get(win);
    if (panels) {
      panels.delete(this._getPanelForNode(aPanel));
    }
  },

  ensureButtonContextMenu: function(aNode, aAreaNode) {
    const kPanelItemContextMenu = "customizationPanelItemContextMenu";

    let currentContextMenu = aNode.getAttribute("context") ||
                             aNode.getAttribute("contextmenu");
    let place = CustomizableUI.getPlaceForItem(aAreaNode);
    let contextMenuForPlace = place == "panel" ?
                                kPanelItemContextMenu :
                                null;
    if (contextMenuForPlace && !currentContextMenu) {
      aNode.setAttribute("context", contextMenuForPlace);
    } else if (currentContextMenu == kPanelItemContextMenu &&
               contextMenuForPlace != kPanelItemContextMenu) {
      aNode.removeAttribute("context");
      aNode.removeAttribute("contextmenu");
    }
  },

  getWidgetProvider: function(aWidgetId) {
    if (this.isSpecialWidget(aWidgetId)) {
      return CustomizableUI.PROVIDER_SPECIAL;
    }
    if (gPalette.has(aWidgetId)) {
      return CustomizableUI.PROVIDER_API;
    }
    // If this was an API widget that was destroyed, return null:
    if (gSeenWidgets.has(aWidgetId)) {
      return null;
    }

    // We fall back to the XUL provider, but we don't know for sure (at this
    // point) whether it exists there either. So the API is technically lying.
    // Ideally, it would be able to return an error value (or throw an
    // exception) if it really didn't exist. Our code calling this function
    // handles that fine, but this is a public API.
    return CustomizableUI.PROVIDER_XUL;
  },

  getWidgetNode: function(aWidgetId, aWindow) {
    let document = aWindow.document;

    if (this.isSpecialWidget(aWidgetId)) {
      let widgetNode = document.getElementById(aWidgetId) ||
                       this.createSpecialWidget(aWidgetId, document);
      return [ CustomizableUI.PROVIDER_SPECIAL, widgetNode];
    }

    let widget = gPalette.get(aWidgetId);
    if (widget) {
      // If we have an instance of this widget already, just use that.
      if (widget.instances.has(document)) {
        LOG("An instance of widget " + aWidgetId + " already exists in this "
            + "document. Reusing.");
        return [ CustomizableUI.PROVIDER_API,
                 widget.instances.get(document) ];
      }

      return [ CustomizableUI.PROVIDER_API,
               this.buildWidget(document, widget) ];
    }

    LOG("Searching for " + aWidgetId + " in toolbox.");
    let node = this.findWidgetInWindow(aWidgetId, aWindow);
    if (node) {
      return [ CustomizableUI.PROVIDER_XUL, node ];
    }

    LOG("No node for " + aWidgetId + " found.");
    return [null, null];
  },

  registerMenuPanel: function(aPanelContents) {
    if (gBuildAreas.has(CustomizableUI.AREA_PANEL) &&
        gBuildAreas.get(CustomizableUI.AREA_PANEL).has(aPanelContents)) {
      return;
    }

    let document = aPanelContents.ownerDocument;

    aPanelContents.toolbox = document.getElementById("navigator-toolbox");
    aPanelContents.customizationTarget = aPanelContents;

    this.addPanelCloseListeners(this._getPanelForNode(aPanelContents));

    let placements = gPlacements.get(CustomizableUI.AREA_PANEL);
    this.buildArea(CustomizableUI.AREA_PANEL, placements, aPanelContents);
    for (let child of aPanelContents.children) {
      if (child.localName != "toolbarbutton") {
        if (child.localName == "toolbaritem") {
          this.ensureButtonContextMenu(child, aPanelContents);
        }
        continue;
      }
      this.ensureButtonContextMenu(child, aPanelContents);
      child.setAttribute("wrap", "true");
    }

    this.registerBuildArea(CustomizableUI.AREA_PANEL, aPanelContents);
  },

  onWidgetAdded: function(aWidgetId, aArea, aPosition) {
    this.insertNode(aWidgetId, aArea, aPosition, true);
  },

  onWidgetRemoved: function(aWidgetId, aArea) {
    let areaNodes = gBuildAreas.get(aArea);
    if (!areaNodes) {
      return;
    }

    let area = gAreas.get(aArea);
    let isToolbar = area.get("type") == CustomizableUI.TYPE_TOOLBAR;
    let isOverflowable = isToolbar && area.get("overflowable");
    let showInPrivateBrowsing = gPalette.has(aWidgetId)
                              ? gPalette.get(aWidgetId).showInPrivateBrowsing
                              : true;

    for (let areaNode of areaNodes) {
      let window = areaNode.ownerDocument.defaultView;
      if (!showInPrivateBrowsing &&
          PrivateBrowsingUtils.isWindowPrivate(window)) {
        continue;
      }

      let widgetNode = window.document.getElementById(aWidgetId);
      if (!widgetNode) {
        ERROR("Widget not found, unable to remove");
        continue;
      }
      let container = areaNode.customizationTarget;
      if (isOverflowable) {
        container = areaNode.overflowable.getContainerFor(widgetNode);
      }

      this.notifyListeners("onWidgetBeforeDOMChange", widgetNode, null, container, true);

      // We remove location attributes here to make sure they're gone too when a
      // widget is removed from a toolbar to the palette. See bug 930950.
      this.removeLocationAttributes(widgetNode);
      // We also need to remove the panel context menu if it's there:
      this.ensureButtonContextMenu(widgetNode);
      widgetNode.removeAttribute("wrap");
      if (gPalette.has(aWidgetId) || this.isSpecialWidget(aWidgetId)) {
        container.removeChild(widgetNode);
      } else {
        areaNode.toolbox.palette.appendChild(widgetNode);
      }
      this.notifyListeners("onWidgetAfterDOMChange", widgetNode, null, container, true);

      if (isToolbar) {
        areaNode.setAttribute("currentset", gPlacements.get(aArea).join(','));
      }

      let windowCache = gSingleWrapperCache.get(window);
      if (windowCache) {
        windowCache.delete(aWidgetId);
      }
    }
  },

  onWidgetMoved: function(aWidgetId, aArea, aOldPosition, aNewPosition) {
    this.insertNode(aWidgetId, aArea, aNewPosition);
  },

  registerBuildArea: function(aArea, aNode) {
    // We ensure that the window is registered to have its customization data
    // cleaned up when unloading.
    let window = aNode.ownerDocument.defaultView;
    if (window.closed) {
      return;
    }
    this.registerBuildWindow(window);

    // Also register this build area's toolbox.
    if (aNode.toolbox) {
      gBuildWindows.get(window).add(aNode.toolbox);
    }

    if (!gBuildAreas.has(aArea)) {
      gBuildAreas.set(aArea, new Set());
    }

    gBuildAreas.get(aArea).add(aNode);
  },

  registerBuildWindow: function(aWindow) {
    if (!gBuildWindows.has(aWindow)) {
      gBuildWindows.set(aWindow, new Set());

      aWindow.addEventListener("unload", this);
      aWindow.addEventListener("command", this, true);
    }
  },

  unregisterBuildWindow: function(aWindow) {
    aWindow.removeEventListener("unload", this);
    aWindow.removeEventListener("command", this, true);
    gPanelsForWindow.delete(aWindow);
    gBuildWindows.delete(aWindow);
    gSingleWrapperCache.delete(aWindow);
    let document = aWindow.document;

    for (let [areaId, areaNodes] of gBuildAreas) {
      let areaProperties = gAreas.get(areaId);
      for (let node of areaNodes) {
        if (node.ownerDocument == document) {
          if (areaProperties.has("overflowable")) {
            node.overflowable.uninit();
            node.overflowable = null;
          }
          areaNodes.delete(node);
        }
      }
    }

    for (let [,widget] of gPalette) {
      widget.instances.delete(document);
      this.notifyListeners("onWidgetInstanceRemoved", widget.id, document);
    }

    for (let [area, areaMap] of gPendingBuildAreas) {
      let toDelete = [];
      for (let [areaNode, ] of areaMap) {
        if (areaNode.ownerDocument == document) {
          toDelete.push(areaNode);
        }
      }
      for (let areaNode of toDelete) {
        areaMap.delete(toDelete);
      }
    }
  },

  setLocationAttributes: function(aNode, aArea) {
    let props = gAreas.get(aArea);
    if (!props) {
      throw new Error("Expected area " + aArea + " to have a properties Map " +
                      "associated with it.");
    }

    aNode.setAttribute("cui-areatype", props.get("type") || "");
    let anchor = props.get("anchor");
    if (anchor) {
      aNode.setAttribute("cui-anchorid", anchor);
    }
  },

  removeLocationAttributes: function(aNode) {
    aNode.removeAttribute("cui-areatype");
    aNode.removeAttribute("cui-anchorid");
  },

  insertNode: function(aWidgetId, aArea, aPosition, isNew) {
    let areaNodes = gBuildAreas.get(aArea);
    if (!areaNodes) {
      return;
    }

    let placements = gPlacements.get(aArea);
    if (!placements) {
      ERROR("Could not find any placements for " + aArea +
            " when moving a widget.");
      return;
    }

    let nextNodeId = placements[aPosition + 1];
    // Go through each of the nodes associated with this area and move the
    // widget to the requested location.
    for (let areaNode of areaNodes) {
      this.insertNodeInWindow(aWidgetId, areaNode, nextNodeId, isNew);
    }
  },

  insertNodeInWindow: function(aWidgetId, aAreaNode, aNextNodeId, isNew) {
    let window = aAreaNode.ownerDocument.defaultView;
    let showInPrivateBrowsing = gPalette.has(aWidgetId)
                              ? gPalette.get(aWidgetId).showInPrivateBrowsing
                              : true;

    if (!showInPrivateBrowsing && PrivateBrowsingUtils.isWindowPrivate(window)) {
      return;
    }

    let [, widgetNode] = this.getWidgetNode(aWidgetId, window);
    if (!widgetNode) {
      ERROR("Widget '" + aWidgetId + "' not found, unable to move");
      return;
    }

    let areaId = aAreaNode.id;
    if (isNew) {
      this.ensureButtonContextMenu(widgetNode, aAreaNode);
      if (widgetNode.localName == "toolbarbutton" && areaId == CustomizableUI.AREA_PANEL) {
        widgetNode.setAttribute("wrap", "true");
      }
    }

    let container = aAreaNode.customizationTarget;
    let [insertionContainer, nextNode] = this.findInsertionPoints(widgetNode, aNextNodeId, aAreaNode);
    this.insertWidgetBefore(widgetNode, nextNode, insertionContainer, areaId);

    if (gAreas.get(areaId).get("type") == CustomizableUI.TYPE_TOOLBAR) {
      aAreaNode.setAttribute("currentset", gPlacements.get(areaId).join(','));
    }
  },

  findInsertionPoints: function(aNode, aNextNodeId, aAreaNode) {
    let props = gAreas.get(aAreaNode.id);
    if (props.get("type") == CustomizableUI.TYPE_TOOLBAR && props.get("overflowable") &&
        aAreaNode.getAttribute("overflowing") == "true") {
      return aAreaNode.overflowable.getOverflowedInsertionPoints(aNode, aNextNodeId);
    }
    let nextNode = null;
    if (aNextNodeId) {
      nextNode = aAreaNode.customizationTarget.getElementsByAttribute("id", aNextNodeId)[0];
    }
    return [aAreaNode.customizationTarget, nextNode];
  },

  insertWidgetBefore: function(aNode, aNextNode, aContainer, aArea) {
    this.notifyListeners("onWidgetBeforeDOMChange", aNode, aNextNode, aContainer);
    this.setLocationAttributes(aNode, aArea);
    aContainer.insertBefore(aNode, aNextNode);
    this.notifyListeners("onWidgetAfterDOMChange", aNode, aNextNode, aContainer);
  },

  handleEvent: function(aEvent) {
    switch (aEvent.type) {
      case "command":
        if (!this._originalEventInPanel(aEvent)) {
          break;
        }
        aEvent = aEvent.sourceEvent;
        // Fall through
      case "click":
      case "keypress":
        this.maybeAutoHidePanel(aEvent);
        break;
      case "unload":
        this.unregisterBuildWindow(aEvent.currentTarget);
        break;
    }
  },

  _originalEventInPanel: function(aEvent) {
    let e = aEvent.sourceEvent;
    if (!e) {
      return false;
    }
    let node = this._getPanelForNode(e.target);
    if (!node) {
      return false;
    }
    let win = e.view;
    let panels = gPanelsForWindow.get(win);
    return !!panels && panels.has(node);
  },

  isSpecialWidget: function(aId) {
    return (aId.startsWith(kSpecialWidgetPfx) ||
            aId.startsWith("separator") ||
            aId.startsWith("spring") ||
            aId.startsWith("spacer"));
  },

  ensureSpecialWidgetId: function(aId) {
    let nodeType = aId.match(/spring|spacer|separator/)[0];
    // If the ID we were passed isn't a generated one, generate one now:
    if (nodeType == aId) {
      // Ids are differentiated through a unique count suffix.
      return kSpecialWidgetPfx + aId + (++gNewElementCount);
    }
    return aId;
  },

  createSpecialWidget: function(aId, aDocument) {
    let nodeName = "toolbar" + aId.match(/spring|spacer|separator/)[0];
    let node = aDocument.createElementNS(kNSXUL, nodeName);
    node.id = this.ensureSpecialWidgetId(aId);
    if (nodeName == "toolbarspring") {
      node.flex = 1;
    }
    return node;
  },

  /* Find a XUL-provided widget in a window. Don't try to use this
   * for an API-provided widget or a special widget.
   */
  findWidgetInWindow: function(aId, aWindow) {
    if (!gBuildWindows.has(aWindow)) {
      throw new Error("Build window not registered");
    }

    if (!aId) {
      ERROR("findWidgetInWindow was passed an empty string.");
      return null;
    }

    let document = aWindow.document;

    // look for a node with the same id, as the node may be
    // in a different toolbar.
    let node = document.getElementById(aId);
    if (node) {
      let parent = node.parentNode;
      while (parent && !(parent.customizationTarget ||
                         parent == aWindow.gNavToolbox.palette)) {
        parent = parent.parentNode;
      }

      if (parent) {
        let nodeInArea = node.parentNode.localName == "toolbarpaletteitem" ?
                         node.parentNode : node;
        // Check if we're in a customization target, or in the palette:
        if ((parent.customizationTarget == nodeInArea.parentNode &&
             gBuildWindows.get(aWindow).has(parent.toolbox)) ||
            aWindow.gNavToolbox.palette == nodeInArea.parentNode) {
          // Normalize the removable attribute. For backwards compat, if
          // the widget is not located in a toolbox palette then absence
          // of the "removable" attribute means it is not removable.
          if (!node.hasAttribute("removable")) {
            // If we first see this in customization mode, it may be in the
            // customization palette instead of the toolbox palette.
            node.setAttribute("removable", !parent.customizationTarget);
          }
          return node;
        }
      }
    }

    let toolboxes = gBuildWindows.get(aWindow);
    for (let toolbox of toolboxes) {
      if (toolbox.palette) {
        // Attempt to locate a node with a matching ID within
        // the palette.
        let node = toolbox.palette.getElementsByAttribute("id", aId)[0];
        if (node) {
          // Normalize the removable attribute. For backwards compat, this
          // is optional if the widget is located in the toolbox palette,
          // and defaults to *true*, unlike if it was located elsewhere.
          if (!node.hasAttribute("removable")) {
            node.setAttribute("removable", true);
          }
          return node;
        }
      }
    }
    return null;
  },

  buildWidget: function(aDocument, aWidget) {
    if (typeof aWidget == "string") {
      aWidget = gPalette.get(aWidget);
    }
    if (!aWidget) {
      throw new Error("buildWidget was passed a non-widget to build.");
    }

    LOG("Building " + aWidget.id + " of type " + aWidget.type);

    let node;
    if (aWidget.type == "custom") {
      if (aWidget.onBuild) {
        try {
          node = aWidget.onBuild(aDocument);
        } catch (ex) {
          ERROR("Custom widget with id " + aWidget.id + " threw an error: " + ex.message);
        }
      }
      if (!node || !(node instanceof aDocument.defaultView.XULElement))
        ERROR("Custom widget with id " + aWidget.id + " does not return a valid node");
    }
    else {
      node = aDocument.createElementNS(kNSXUL, "toolbarbutton");

      node.setAttribute("id", aWidget.id);
      node.setAttribute("widget-id", aWidget.id);
      node.setAttribute("widget-type", aWidget.type);
      if (aWidget.disabled) {
        node.setAttribute("disabled", true);
      }
      node.setAttribute("removable", aWidget.removable);
      node.setAttribute("overflows", aWidget.overflows);
      node.setAttribute("label", this.getLocalizedProperty(aWidget, "label"));
      let additionalTooltipArguments = [];
      if (aWidget.shortcutId) {
        let keyEl = aDocument.getElementById(aWidget.shortcutId);
        if (keyEl) {
          additionalTooltipArguments.push(ShortcutUtils.prettifyShortcut(keyEl));
        } else {
          ERROR("Key element with id '" + aWidget.shortcutId + "' for widget '" + aWidget.id +
                "' not found!");
        }
      }

      let tooltip = this.getLocalizedProperty(aWidget, "tooltiptext", additionalTooltipArguments);
      node.setAttribute("tooltiptext", tooltip);
      node.setAttribute("class", "toolbarbutton-1 chromeclass-toolbar-additional");

      let commandHandler = this.handleWidgetCommand.bind(this, aWidget, node);
      node.addEventListener("command", commandHandler, false);
      let clickHandler = this.handleWidgetClick.bind(this, aWidget, node);
      node.addEventListener("click", clickHandler, false);

      // If the widget has a view, and has view showing / hiding listeners,
      // hook those up to this widget.
      if (aWidget.type == "view") {
        LOG("Widget " + aWidget.id + " has a view. Auto-registering event handlers.");
        let viewNode = aDocument.getElementById(aWidget.viewId);

        if (viewNode) {
          // PanelUI relies on the .PanelUI-subView class to be able to show only
          // one sub-view at a time.
          viewNode.classList.add("PanelUI-subView");

          for (let eventName of kSubviewEvents) {
            let handler = "on" + eventName;
            if (typeof aWidget[handler] == "function") {
              viewNode.addEventListener(eventName, aWidget[handler], false);
            }
          }

          LOG("Widget " + aWidget.id + " showing and hiding event handlers set.");
        } else {
          ERROR("Could not find the view node with id: " + aWidget.viewId +
                ", for widget: " + aWidget.id + ".");
        }
      }

      if (aWidget.onCreated) {
        aWidget.onCreated(node);
      }
    }

    aWidget.instances.set(aDocument, node);
    return node;
  },

  getLocalizedProperty: function(aWidget, aProp, aFormatArgs, aDef) {
    if (typeof aWidget == "string") {
      aWidget = gPalette.get(aWidget);
    }
    if (!aWidget) {
      throw new Error("getLocalizedProperty was passed a non-widget to work with.");
    }
    let def, name;
    // Let widgets pass their own string identifiers or strings, so that
    // we can use strings which aren't the default (in case string ids change)
    // and so that non-builtin-widgets can also provide labels, tooltips, etc.
    if (aWidget[aProp]) {
      name = aWidget[aProp];
      // By using this as the default, if a widget provides a full string rather
      // than a string ID for localization, we will fall back to that string
      // and return that.
      def = aDef || name;
    } else {
      name = aWidget.id + "." + aProp;
      def = aDef || "";
    }
    try {
      if (Array.isArray(aFormatArgs) && aFormatArgs.length) {
        return gWidgetsBundle.formatStringFromName(name, aFormatArgs,
          aFormatArgs.length) || def;
      }
      return gWidgetsBundle.GetStringFromName(name) || def;
    } catch(ex) {
      if (!def) {
        ERROR("Could not localize property '" + name + "'.");
      }
    }
    return def;
  },

  handleWidgetCommand: function(aWidget, aNode, aEvent) {
    LOG("handleWidgetCommand");

    if (aWidget.type == "button") {
      if (aWidget.onCommand) {
        try {
          aWidget.onCommand.call(null, aEvent);
        } catch (e) {
          ERROR(e);
        }
      } else {
        //XXXunf Need to think this through more, and formalize.
        Services.obs.notifyObservers(aNode,
                                     "customizedui-widget-command",
                                     aWidget.id);
      }
    } else if (aWidget.type == "view") {
      let ownerWindow = aNode.ownerDocument.defaultView;
      ownerWindow.PanelUI.showSubView(aWidget.viewId, aNode,
                                      this.getPlacementOfWidget(aNode.id).area);
    }
  },

  handleWidgetClick: function(aWidget, aNode, aEvent) {
    LOG("handleWidgetClick");
    if (aWidget.onClick) {
      try {
        aWidget.onClick.call(null, aEvent);
      } catch(e) {
        Cu.reportError(e);
      }
    } else {
      //XXXunf Need to think this through more, and formalize.
      Services.obs.notifyObservers(aNode, "customizedui-widget-click", aWidget.id);
    }
  },

  _getPanelForNode: function(aNode) {
    let panel = aNode;
    while (panel && panel.localName != "panel")
      panel = panel.parentNode;
    return panel;
  },

  /*
   * If people put things in the panel which need more than single-click interaction,
   * we don't want to close it. Right now we check for text inputs and menu buttons.
   * We also check for being outside of any toolbaritem/toolbarbutton, ie on a blank
   * part of the menu.
   */
  _isOnInteractiveElement: function(aEvent) {
    function getMenuPopupForDescendant(aNode) {
      let lastPopup = null;
      while (aNode && aNode.parentNode &&
             aNode.parentNode.localName.startsWith("menu")) {
        lastPopup = aNode.localName == "menupopup" ? aNode : lastPopup;
        aNode = aNode.parentNode;
      }
      return lastPopup;
    }

    let target = aEvent.originalTarget;
    let panel = this._getPanelForNode(aEvent.currentTarget);
    // This can happen in e.g. customize mode. If there's no panel,
    // there's clearly nothing for us to close; pretend we're interactive.
    if (!panel) {
      return true;
    }
    // We keep track of:
    // whether we're in an input container (text field)
    let inInput = false;
    // whether we're in a popup/context menu
    let inMenu = false;
    // whether we're in a toolbarbutton/toolbaritem
    let inItem = false;
    // whether the current menuitem has a valid closemenu attribute
    let menuitemCloseMenu = "auto";
    // whether the toolbarbutton/item has a valid closemenu attribute.
    let closemenu = "auto";

    // While keeping track of that, we go from the original target back up,
    // to the panel if we have to. We bail as soon as we find an input,
    // a toolbarbutton/item, or the panel:
    while (true) {
      let tagName = target.localName;
      inInput = tagName == "input" || tagName == "textbox";
      inItem = tagName == "toolbaritem" || tagName == "toolbarbutton";
      let isMenuItem = tagName == "menuitem";
      inMenu = inMenu || isMenuItem;
      if (inItem && target.hasAttribute("closemenu")) {
        let closemenuVal = target.getAttribute("closemenu");
        closemenu = (closemenuVal == "single" || closemenuVal == "none") ?
                    closemenuVal : "auto";
      }

      if (isMenuItem && target.hasAttribute("closemenu")) {
        let closemenuVal = target.getAttribute("closemenu");
        menuitemCloseMenu = (closemenuVal == "single" || closemenuVal == "none") ?
                            closemenuVal : "auto";
      }
      // This isn't in the loop condition because we want to break before
      // changing |target| if any of these conditions are true
      if (inInput || inItem || target == panel) {
        break;
      }
      // We need specific code for popups: the item on which they were invoked
      // isn't necessarily in their parentNode chain:
      if (isMenuItem) {
        let topmostMenuPopup = getMenuPopupForDescendant(target);
        target = (topmostMenuPopup && topmostMenuPopup.triggerNode) ||
                 target.parentNode;
      } else {
        target = target.parentNode;
      }
    }
    // If the user clicked a menu item...
    if (inMenu) {
      // We care if we're in an input also,
      // or if the user specified closemenu!="auto":
      if (inInput || menuitemCloseMenu != "auto") {
        return true;
      }
      // Otherwise, we're probably fine to close the panel
      return false;
    }
    // If we're not in a menu, and we *are* in a type="menu" toolbarbutton,
    // we'll now interact with the menu
    if (inItem && target.getAttribute("type") == "menu") {
      return true;
    }
    // If we're not in a menu, and we *are* in a type="menu-button" toolbarbutton,
    // it depends whether we're in the dropmarker or the 'real' button:
    if (inItem && target.getAttribute("type") == "menu-button") {
      // 'real' button (which has a single action):
      if (target.getAttribute("anonid") == "button") {
        return closemenu != "none";
      }
      // otherwise, this is the outer button, and the user will now
      // interact with the menu:
      return true;
    }
    return inInput || !inItem;
  },

  hidePanelForNode: function(aNode) {
    let panel = this._getPanelForNode(aNode);
    if (panel) {
      panel.hidePopup();
    }
  },

  maybeAutoHidePanel: function(aEvent) {
    if (aEvent.type == "keypress") {
      if (aEvent.keyCode != aEvent.DOM_VK_ENTER &&
          aEvent.keyCode != aEvent.DOM_VK_RETURN) {
        return;
      }
      // If the user hit enter/return, we don't check preventDefault - it makes sense
      // that this was prevented, but we probably still want to close the panel.
      // If consumers don't want this to happen, they should specify the closemenu
      // attribute.

    } else if (aEvent.type != "command") { // mouse events:
      if (aEvent.defaultPrevented || aEvent.button != 0) {
        return;
      }
      let isInteractive = this._isOnInteractiveElement(aEvent);
      LOG("maybeAutoHidePanel: interactive ? " + isInteractive);
      if (isInteractive) {
        return;
      }
    }

    // We can't use event.target because we might have passed a panelview
    // anonymous content boundary as well, and so target points to the
    // panelmultiview in that case. Unfortunately, this means we get
    // anonymous child nodes instead of the real ones, so looking for the 
    // 'stoooop, don't close me' attributes is more involved.
    let target = aEvent.originalTarget;
    let closemenu = "auto";
    let widgetType = "button";
    while (target.parentNode && target.localName != "panel") {
      closemenu = target.getAttribute("closemenu");
      widgetType = target.getAttribute("widget-type");
      if (closemenu == "none" || closemenu == "single" ||
          widgetType == "view") {
        break;
      }
      target = target.parentNode;
    }
    if (closemenu == "none" || widgetType == "view") {
      return;
    }

    if (closemenu == "single") {
      let panel = this._getPanelForNode(target);
      let multiview = panel.querySelector("panelmultiview");
      if (multiview.showingSubView) {
        multiview.showMainView();
        return;
      }
    }

    // If we get here, we can actually hide the popup:
    this.hidePanelForNode(aEvent.target);
  },

  getUnusedWidgets: function(aWindowPalette) {
    let window = aWindowPalette.ownerDocument.defaultView;
    let isWindowPrivate = PrivateBrowsingUtils.isWindowPrivate(window);
    // We use a Set because there can be overlap between the widgets in
    // gPalette and the items in the palette, especially after the first
    // customization, since programmatically generated widgets will remain
    // in the toolbox palette.
    let widgets = new Set();

    // It's possible that some widgets have been defined programmatically and
    // have not been overlayed into the palette. We can find those inside
    // gPalette.
    for (let [id, widget] of gPalette) {
      if (!widget.currentArea) {
        if (widget.showInPrivateBrowsing || !isWindowPrivate) {
          widgets.add(id);
        }
      }
    }

    LOG("Iterating the actual nodes of the window palette");
    for (let node of aWindowPalette.children) {
      LOG("In palette children: " + node.id);
      if (node.id && !this.getPlacementOfWidget(node.id)) {
        widgets.add(node.id);
      }
    }

    return [...widgets];
  },

  getPlacementOfWidget: function(aWidgetId, aOnlyRegistered, aDeadAreas) {
    if (aOnlyRegistered && !this.widgetExists(aWidgetId)) {
      return null;
    }

    for (let [area, placements] of gPlacements) {
      if (!gAreas.has(area) && !aDeadAreas) {
        continue;
      }
      let index = placements.indexOf(aWidgetId);
      if (index != -1) {
        return { area: area, position: index };
      }
    }

    return null;
  },

  widgetExists: function(aWidgetId) {
    if (gPalette.has(aWidgetId) || this.isSpecialWidget(aWidgetId)) {
      return true;
    }

    // Destroyed API widgets are in gSeenWidgets, but not in gPalette:
    if (gSeenWidgets.has(aWidgetId)) {
      return false;
    }

    // We're assuming XUL widgets always exist, as it's much harder to check,
    // and checking would be much more error prone.
    return true;
  },

  addWidgetToArea: function(aWidgetId, aArea, aPosition, aInitialAdd) {
    if (!gAreas.has(aArea)) {
      throw new Error("Unknown customization area: " + aArea);
    }

    // If this is a lazy area that hasn't been restored yet, we can't yet modify
    // it - would would at least like to add to it. So we keep track of it in
    // gFuturePlacements,  and use that to add it when restoring the area. We
    // throw away aPosition though, as that can only be bogus if the area hasn't
    // yet been restorted (caller can't possibly know where its putting the
    // widget in relation to other widgets).
    if (this.isAreaLazy(aArea)) {
      gFuturePlacements.get(aArea).add(aWidgetId);
      return;
    }

    if (this.isSpecialWidget(aWidgetId)) {
      aWidgetId = this.ensureSpecialWidgetId(aWidgetId);
    }

    let oldPlacement = this.getPlacementOfWidget(aWidgetId, false, true);
    if (oldPlacement && oldPlacement.area == aArea) {
      this.moveWidgetWithinArea(aWidgetId, aPosition);
      return;
    }

    // Do nothing if the widget is not allowed to move to the target area.
    if (!this.canWidgetMoveToArea(aWidgetId, aArea)) {
      return;
    }

    if (oldPlacement) {
      this.removeWidgetFromArea(aWidgetId);
    }

    if (!gPlacements.has(aArea)) {
      gPlacements.set(aArea, [aWidgetId]);
      aPosition = 0;
    } else {
      let placements = gPlacements.get(aArea);
      if (typeof aPosition != "number") {
        aPosition = placements.length;
      }
      if (aPosition < 0) {
        aPosition = 0;
      }
      placements.splice(aPosition, 0, aWidgetId);
    }

    let widget = gPalette.get(aWidgetId);
    if (widget) {
      widget.currentArea = aArea;
      widget.currentPosition = aPosition;
    }

    // We initially set placements with addWidgetToArea, so in that case
    // we don't consider the area "dirtied".
    if (!aInitialAdd) {
      gDirtyAreaCache.add(aArea);
    }

    gDirty = true;
    this.saveState();

    this.notifyListeners("onWidgetAdded", aWidgetId, aArea, aPosition);
  },

  removeWidgetFromArea: function(aWidgetId) {
    let oldPlacement = this.getPlacementOfWidget(aWidgetId, false, true);
    if (!oldPlacement) {
      return;
    }

    if (!this.isWidgetRemovable(aWidgetId)) {
      return;
    }

    let placements = gPlacements.get(oldPlacement.area);
    let position = placements.indexOf(aWidgetId);
    if (position != -1) {
      placements.splice(position, 1);
    }

    let widget = gPalette.get(aWidgetId);
    if (widget) {
      widget.currentArea = null;
      widget.currentPosition = null;
    }

    gDirty = true;
    this.saveState();
    gDirtyAreaCache.add(oldPlacement.area);

    this.notifyListeners("onWidgetRemoved", aWidgetId, oldPlacement.area);
  },

  moveWidgetWithinArea: function(aWidgetId, aPosition) {
    let oldPlacement = this.getPlacementOfWidget(aWidgetId);
    if (!oldPlacement) {
      return;
    }

    let placements = gPlacements.get(oldPlacement.area);
    if (typeof aPosition != "number") {
      aPosition = placements.length;
    } else if (aPosition < 0) {
      aPosition = 0;
    } else if (aPosition > placements.length) {
      aPosition = placements.length;
    }

    if (aPosition == oldPlacement.position) {
      return;
    }

    placements.splice(oldPlacement.position, 1);
    // If we just removed the item from *before* where it is now added,
    // we need to compensate the position offset for that:
    if (oldPlacement.position < aPosition) {
      aPosition--;
    }
    placements.splice(aPosition, 0, aWidgetId);

    let widget = gPalette.get(aWidgetId);
    if (widget) {
      widget.currentPosition = aPosition;
    }

    gDirty = true;
    gDirtyAreaCache.add(oldPlacement.area);

    this.saveState();

    this.notifyListeners("onWidgetMoved", aWidgetId, oldPlacement.area,
                         oldPlacement.position, aPosition);
  },

  // Note that this does not populate gPlacements, which is done lazily so that
  // the legacy state can be migrated, which is only available once a browser
  // window is openned.
  // The panel area is an exception here, since it has no legacy state and is 
  // built lazily - and therefore wouldn't otherwise result in restoring its
  // state immediately when a browser window opens, which is important for
  // other consumers of this API.
  loadSavedState: function() {
    let state = null;
    try {
      state = Services.prefs.getCharPref(kPrefCustomizationState);
    } catch (e) {
      LOG("No saved state found");
      // This will fail if nothing has been customized, so silently fall back to
      // the defaults.
    }

    if (!state) {
      return;
    }
    try {
      gSavedState = JSON.parse(state);
    } catch(e) {
      LOG("Error loading saved UI customization state, falling back to defaults.");
    }

    if (!("placements" in gSavedState)) {
      gSavedState.placements = {};
    }

    gSeenWidgets = new Set(gSavedState.seen || []);
    gDirtyAreaCache = new Set(gSavedState.dirtyAreaCache || []);
    gNewElementCount = gSavedState.newElementCount || 0;
  },

  restoreStateForArea: function(aArea, aLegacyState) {
    if (gPlacements.has(aArea)) {
      // Already restored.
      return;
    }

    this.beginBatchUpdate();
    try {
      gRestoring = true;

      let restored = false;
      gPlacements.set(aArea, []);

      if (gSavedState && aArea in gSavedState.placements) {
        LOG("Restoring " + aArea + " from saved state");
        let placements = gSavedState.placements[aArea];
        for (let id of placements)
          this.addWidgetToArea(id, aArea);
        gDirty = false;
        restored = true;
      }

      if (!restored && aLegacyState) {
        LOG("Restoring " + aArea + " from legacy state");
        for (let id of aLegacyState)
          this.addWidgetToArea(id, aArea);
        // Don't override dirty state, to ensure legacy state is saved here and
        // therefore only used once.
        restored = true;
      }

      if (!restored) {
        LOG("Restoring " + aArea + " from default state");
        let defaults = gAreas.get(aArea).get("defaultPlacements");
        if (defaults) {
          for (let id of defaults)
            this.addWidgetToArea(id, aArea, null, true);
        }
        gDirty = false;
      }

      // Finally, add widgets to the area that were added before the it was able
      // to be restored. This can occur when add-ons register widgets for a
      // lazily-restored area before it's been restored.
      if (gFuturePlacements.has(aArea)) {
        for (let id of gFuturePlacements.get(aArea))
          this.addWidgetToArea(id, aArea);
      }

      LOG("Placements for " + aArea + ":\n\t" + gPlacements.get(aArea).join("\n\t"));

      gRestoring = false;
    } finally {
      this.endBatchUpdate();
    }
  },

  saveState: function() {
    if (gInBatchStack || !gDirty) {
      return;
    }
    let state = { placements: gPlacements,
                  seen: gSeenWidgets,
                  dirtyAreaCache: gDirtyAreaCache,
                  newElementCount: gNewElementCount };

    LOG("Saving state.");
    let serialized = JSON.stringify(state, this.serializerHelper);
    LOG("State saved as: " + serialized);
    Services.prefs.setCharPref(kPrefCustomizationState, serialized);
    gDirty = false;
  },

  serializerHelper: function(aKey, aValue) {
    if (typeof aValue == "object" && aValue.constructor.name == "Map") {
      let result = {};
      for (let [mapKey, mapValue] of aValue)
        result[mapKey] = mapValue;
      return result;
    }

    if (typeof aValue == "object" && aValue.constructor.name == "Set") {
      return [...aValue];
    }

    return aValue;
  },

  beginBatchUpdate: function() {
    gInBatchStack++;
  },

  endBatchUpdate: function(aForceDirty) {
    gInBatchStack--;
    if (aForceDirty === true) {
      gDirty = true;
    }
    if (gInBatchStack == 0) {
      this.saveState();
    } else if (gInBatchStack < 0) {
      throw new Error("The batch editing stack should never reach a negative number.");
    }
  },

  addListener: function(aListener) {
    gListeners.add(aListener);
  },

  removeListener: function(aListener) {
    if (aListener == this) {
      return;
    }

    gListeners.delete(aListener);
  },

  notifyListeners: function(aEvent, ...aArgs) {
    if (gRestoring) {
      return;
    }

    for (let listener of gListeners) {
      try {
        if (typeof listener[aEvent] == "function") {
          listener[aEvent].apply(listener, aArgs);
        }
      } catch (e) {
        ERROR(e + " -- " + e.fileName + ":" + e.lineNumber);
      }
    }
  },

  createWidget: function(aProperties) {
    let widget = this.normalizeWidget(aProperties, CustomizableUI.SOURCE_EXTERNAL);
    //XXXunf This should probably throw.
    if (!widget) {
      return;
    }

    gPalette.set(widget.id, widget);

    // Clear our caches:
    gGroupWrapperCache.delete(widget.id);
    for (let [win, ] of gBuildWindows) {
      let cache = gSingleWrapperCache.get(win);
      if (cache) {
        cache.delete(widget.id);
      }
    }

    this.notifyListeners("onWidgetCreated", widget.id);

    if (widget.defaultArea) {
      let area = gAreas.get(widget.defaultArea);
      //XXXgijs this won't have any effect for legacy items. Sort of OK because
      // consumers can modify currentset? Maybe?
      if (area.has("defaultPlacements")) {
        area.get("defaultPlacements").push(widget.id);
      } else {
        area.set("defaultPlacements", [widget.id]);
      }
    }

    // Look through previously saved state to see if we're restoring a widget.
    let seenAreas = new Set();
    for (let [area, placements] of gPlacements) {
      seenAreas.add(area);
      let index = gPlacements.get(area).indexOf(widget.id);
      if (index != -1) {
        widget.currentArea = area;
        widget.currentPosition = index;
        break;
      }
    }

    // Also look at saved state data directly in areas that haven't yet been
    // restored. Can't rely on this for restored areas, as they may have
    // changed.
    if (!widget.currentArea && gSavedState) {
      for (let area of Object.keys(gSavedState.placements)) {
        if (seenAreas.has(area)) {
          continue;
        }

        let index = gSavedState.placements[area].indexOf(widget.id);
        if (index != -1) {
          widget.currentArea = area;
          widget.currentPosition = index;
          break;
        }
      }
    }

    // If we're restoring the widget to it's old placement, fire off the
    // onWidgetAdded event - our own handler will take care of adding it to
    // any build areas.
    if (widget.currentArea) {
      this.notifyListeners("onWidgetAdded", widget.id, widget.currentArea,
                           widget.currentPosition);
    } else {
      let autoAdd = true;
      try {
        autoAdd = Services.prefs.getBoolPref(kPrefCustomizationAutoAdd);
      } catch (e) {}

      // If the widget doesn't have an existing placement, and it hasn't been
      // seen before, then add it to its default area so it can be used.
      // If the widget is not removable, we *have* to add it to its default
      // area here.
      let canBeAutoAdded = autoAdd && !gSeenWidgets.has(widget.id);
      if (!widget.currentArea && (!widget.removable || canBeAutoAdded)) {
        this.beginBatchUpdate();
        try {
          gSeenWidgets.add(widget.id);

          if (widget.defaultArea) {
            if (this.isAreaLazy(widget.defaultArea)) {
              gFuturePlacements.get(widget.defaultArea).add(widget.id);
            } else {
              this.addWidgetToArea(widget.id, widget.defaultArea);
            }
          }
        } finally {
          this.endBatchUpdate(true);
        }
      }
    }

    this.notifyListeners("onWidgetAfterCreation", widget.id, widget.currentArea);
    return widget.id;
  },

  createBuiltinWidget: function(aData) {
    // This should only ever be called on startup, before any windows are
    // opened - so we know there's no build areas to handle. Also, builtin
    // widgets are expected to be (mostly) static, so shouldn't affect the
    // current placement settings.
    let widget = this.normalizeWidget(aData, CustomizableUI.SOURCE_BUILTIN);
    if (!widget) {
      ERROR("Error creating builtin widget: " + aData.id);
      return;
    }

    LOG("Creating built-in widget with id: " + widget.id);
    gPalette.set(widget.id, widget);
  },

  // Returns true if the area will eventually lazily restore (but hasn't yet).
  isAreaLazy: function(aArea) {
    if (gPlacements.has(aArea)) {
      return false;
    }
    return gAreas.get(aArea).has("legacy");
  },

  //XXXunf Log some warnings here, when the data provided isn't up to scratch.
  normalizeWidget: function(aData, aSource) {
    let widget = {
      implementation: aData,
      source: aSource || "addon",
      instances: new Map(),
      currentArea: null,
      removable: true,
      overflows: true,
      defaultArea: null,
      shortcutId: null,
      tooltiptext: null,
      showInPrivateBrowsing: true,
    };

    if (typeof aData.id != "string" || !/^[a-z0-9-_]{1,}$/i.test(aData.id)) {
      ERROR("Given an illegal id in normalizeWidget: " + aData.id);
      return null;
    }

    delete widget.implementation.currentArea;
    widget.implementation.__defineGetter__("currentArea", function() widget.currentArea);

    const kReqStringProps = ["id"];
    for (let prop of kReqStringProps) {
      if (typeof aData[prop] != "string") {
        ERROR("Missing required property '" + prop + "' in normalizeWidget: "
              + aData.id);
        return null;
      }
      widget[prop] = aData[prop];
    }

    const kOptStringProps = ["label", "tooltiptext", "shortcutId"];
    for (let prop of kOptStringProps) {
      if (typeof aData[prop] == "string") {
        widget[prop] = aData[prop];
      }
    }

    const kOptBoolProps = ["removable", "showInPrivateBrowsing", "overflows"];
    for (let prop of kOptBoolProps) {
      if (typeof aData[prop] == "boolean") {
        widget[prop] = aData[prop];
      }
    }

    if (aData.defaultArea && gAreas.has(aData.defaultArea)) {
      widget.defaultArea = aData.defaultArea;
    } else if (!widget.removable) {
      ERROR("Widget '" + widget.id + "' is not removable but does not specify " +
            "a valid defaultArea. That's not possible; it must specify a " +
            "valid defaultArea as well.");
      return null;
    }

    if ("type" in aData && gSupportedWidgetTypes.has(aData.type)) {
      widget.type = aData.type;
    } else {
      widget.type = "button";
    }

    widget.disabled = aData.disabled === true;

    this.wrapWidgetEventHandler("onClick", widget);
    this.wrapWidgetEventHandler("onCreated", widget);

    if (widget.type == "button") {
      widget.onCommand = typeof aData.onCommand == "function" ?
                           aData.onCommand :
                           null;
    } else if (widget.type == "view") {
      if (typeof aData.viewId != "string") {
        ERROR("Expected a string for widget " + widget.id + " viewId, but got "
              + aData.viewId);
        return null;
      }
      widget.viewId = aData.viewId;

      this.wrapWidgetEventHandler("onViewShowing", widget);
      this.wrapWidgetEventHandler("onViewHiding", widget);
    } else if (widget.type == "custom") {
      this.wrapWidgetEventHandler("onBuild", widget);
    }

    if (gPalette.has(widget.id)) {
      return null;
    }

    return widget;
  },

  wrapWidgetEventHandler: function(aEventName, aWidget) {
    if (typeof aWidget.implementation[aEventName] != "function") {
      aWidget[aEventName] = null;
      return;
    }
    aWidget[aEventName] = function(...aArgs) {
      // Wrap inside a try...catch to properly log errors, until bug 862627 is
      // fixed, which in turn might help bug 503244.
      try {
        // Don't copy the function to the normalized widget object, instead
        // keep it on the original object provided to the API so that
        // additional methods can be implemented and used by the event
        // handlers.
        return aWidget.implementation[aEventName].apply(aWidget.implementation,
                                                        aArgs);
      } catch (e) {
        Cu.reportError(e);
      }
    };
  },

  destroyWidget: function(aWidgetId) {
    let widget = gPalette.get(aWidgetId);
    if (!widget) {
      return;
    }

    // Remove it from the default placements of an area if it was added there:
    if (widget.defaultArea) {
      let area = gAreas.get(widget.defaultArea);
      if (area) {
        let defaultPlacements = area.get("defaultPlacements");
        // We can assume this is present because if a widget has a defaultArea,
        // we automatically create a defaultPlacements array for that area.
        let widgetIndex = defaultPlacements.indexOf(aWidgetId);
        if (widgetIndex != -1) {
          defaultPlacements.splice(widgetIndex, 1);
        }
      }
    }

    // This will not remove the widget from gPlacements - we want to keep the
    // setting so the widget gets put back in it's old position if/when it
    // returns.
    for (let [window, ] of gBuildWindows) {
      let windowCache = gSingleWrapperCache.get(window);
      if (windowCache) {
        windowCache.delete(aWidgetId);
      }
      let widgetNode = window.document.getElementById(aWidgetId) ||
                       window.gNavToolbox.palette.getElementsByAttribute("id", aWidgetId)[0];
      if (widgetNode) {
        let container = widgetNode.parentNode
        this.notifyListeners("onWidgetBeforeDOMChange", widgetNode, null,
                             container, true);
        widgetNode.remove();
        this.notifyListeners("onWidgetAfterDOMChange", widgetNode, null,
                             container, true);
      }
      if (widget.type == "view") {
        let viewNode = window.document.getElementById(widget.viewId);
        if (viewNode) {
          for (let eventName of kSubviewEvents) {
            let handler = "on" + eventName;
            if (typeof widget[handler] == "function") {
              viewNode.removeEventListener(eventName, widget[handler], false);
            }
          }
        }
      }
    }

    gPalette.delete(aWidgetId);
    gGroupWrapperCache.delete(aWidgetId);

    this.notifyListeners("onWidgetDestroyed", aWidgetId);
  },

  getCustomizeTargetForArea: function(aArea, aWindow) {
    let buildAreaNodes = gBuildAreas.get(aArea);
    if (!buildAreaNodes) {
      return null;
    }

    for (let node of buildAreaNodes) {
      if (node.ownerDocument.defaultView === aWindow) {
        return node.customizationTarget ? node.customizationTarget : node;
      }
    }

    return null;
  },

  reset: function() {
    gResetting = true;
    Services.prefs.clearUserPref(kPrefCustomizationState);
    LOG("State reset");

    // Reset placements to make restoring default placements possible.
    gPlacements = new Map();
    gDirtyAreaCache = new Set();
    gSeenWidgets = new Set();
    // Clear the saved state to ensure that defaults will be used.
    gSavedState = null;
    // Restore the state for each area to its defaults
    for (let [areaId,] of gAreas) {
      this.restoreStateForArea(areaId);
    }

    // Rebuild each registered area (across windows) to reflect the state that
    // was reset above.
    for (let [areaId, areaNodes] of gBuildAreas) {
      let placements = gPlacements.get(areaId);
      for (let areaNode of areaNodes) {
        this.buildArea(areaId, placements, areaNode);

        let area = gAreas.get(areaId);
        if (area.get("type") == CustomizableUI.TYPE_TOOLBAR) {
          let defaultCollapsed = area.get("defaultCollapsed");
          let win = areaNode.ownerDocument.defaultView;
          win.setToolbarVisibility(areaNode, !defaultCollapsed);
        }
      }
    }
    gResetting = false;
  },

  /**
   * @param {String|Node} aWidget - widget ID or a widget node (preferred for performance).
   * @return {Boolean} whether the widget is removable
   */
  isWidgetRemovable: function(aWidget) {
    let widgetId;
    let widgetNode;
    if (typeof aWidget == "string") {
      widgetId = aWidget;
    } else {
      widgetId = aWidget.id;
      widgetNode = aWidget;
    }
    let provider = this.getWidgetProvider(widgetId);

    if (provider == CustomizableUI.PROVIDER_API) {
      return gPalette.get(widgetId).removable;
    }

    if (provider == CustomizableUI.PROVIDER_XUL) {
      if (gBuildWindows.size == 0) {
        // We don't have any build windows to look at, so just assume for now
        // that its removable.
        return true;
      }

      if (!widgetNode) {
        // Pick any of the build windows to look at.
        let [window,] = [...gBuildWindows][0];
        [, widgetNode] = this.getWidgetNode(widgetId, window);
      }
      // If we don't have a node, we assume it's removable. This can happen because
      // getWidgetProvider returns PROVIDER_XUL by default, but this will also happen
      // for API-provided widgets which have been destroyed.
      if (!widgetNode) {
        return true;
      }
      return widgetNode.getAttribute("removable") == "true";
    }

    // Otherwise this is either a special widget, which is always removable, or
    // an API widget which has already been removed from gPalette. Returning true
    // here allows us to then remove its ID from any placements where it might
    // still occur.
    return true;
  },

  canWidgetMoveToArea: function(aWidgetId, aArea) {
    let placement = this.getPlacementOfWidget(aWidgetId);
    if (placement && placement.area != aArea &&
        !this.isWidgetRemovable(aWidgetId)) {
      return false;
    }
    return true;
  },

  ensureWidgetPlacedInWindow: function(aWidgetId, aWindow) {
    let placement = this.getPlacementOfWidget(aWidgetId);
    if (!placement) {
      return false;
    }
    let areaNodes = gBuildAreas.get(placement.area);
    if (!areaNodes) {
      return false;
    }
    let container = [...areaNodes].filter((n) => n.ownerDocument.defaultView == aWindow);
    if (!container.length) {
      return false;
    }
    let existingNode = container[0].getElementsByAttribute("id", aWidgetId)[0];
    if (existingNode) {
      return true;
    }

    let placementAry = gPlacements.get(placement.area);
    let nextNodeId = placementAry[placement.position + 1];
    this.insertNodeInWindow(aWidgetId, container[0], nextNodeId, true);
    return true;
  },

  get inDefaultState() {
    for (let [areaId, props] of gAreas) {
      let defaultPlacements = props.get("defaultPlacements");
      // Areas without default placements (like legacy ones?) get skipped
      if (!defaultPlacements) {
        continue;
      }

      let currentPlacements = gPlacements.get(areaId);
      // We're excluding all of the placement IDs for items that do not exist,
      // and items that have removable="false",
      // because we don't want to consider them when determining if we're
      // in the default state. This way, if an add-on introduces a widget
      // and is then uninstalled, the leftover placement doesn't cause us to
      // automatically assume that the buttons are not in the default state.
      let buildAreaNodes = gBuildAreas.get(areaId);
      if (buildAreaNodes && buildAreaNodes.size) {
        let container = [...buildAreaNodes][0];
        let removableOrDefault = (itemNodeOrItem) => {
          let item = (itemNodeOrItem && itemNodeOrItem.id) || itemNodeOrItem;
          let isRemovable = this.isWidgetRemovable(itemNodeOrItem);
          let isInDefault = defaultPlacements.indexOf(item) != -1;
          return isRemovable || isInDefault;
        };
        // Toolbars have a currentSet property which also deals correctly with overflown
        // widgets (if any) - use that instead:
        if (props.get("type") == CustomizableUI.TYPE_TOOLBAR) {
          currentPlacements = container.currentSet.split(',');
          currentPlacements = currentPlacements.filter(removableOrDefault);
        } else {
          // Clone the array so we don't modify the actual placements...
          currentPlacements = [...currentPlacements];
          currentPlacements = currentPlacements.filter((item) => {
            let itemNode = container.getElementsByAttribute("id", item)[0];
            return itemNode && removableOrDefault(itemNode || item);
          });
        }

        if (props.get("type") == CustomizableUI.TYPE_TOOLBAR) {
          let attribute = container.getAttribute("type") == "menubar" ? "autohide" : "collapsed";
          let collapsed = container.getAttribute(attribute) == "true";
          let defaultCollapsed = props.get("defaultCollapsed");
          if (collapsed != defaultCollapsed) {
            LOG("Found " + areaId + " had non-default toolbar visibility (expected " + defaultCollapsed + ", was " + collapsed + ")");
            return false;
          }
        }
      }
      LOG("Checking default state for " + areaId + ":\n" + currentPlacements.join(",") +
          "\nvs.\n" + defaultPlacements.join(","));

      if (currentPlacements.length != defaultPlacements.length) {
        return false;
      }

      for (let i = 0; i < currentPlacements.length; ++i) {
        if (currentPlacements[i] != defaultPlacements[i]) {
          LOG("Found " + currentPlacements[i] + " in " + areaId + " where " +
              defaultPlacements[i] + " was expected!");
          return false;
        }
      }
    }

    return true;
  }
};
Object.freeze(CustomizableUIInternal);

this.CustomizableUI = {
  /**
   * Constant reference to the ID of the menu panel.
   */
  get AREA_PANEL() "PanelUI-contents",
  /**
   * Constant reference to the ID of the navigation toolbar.
   */
  get AREA_NAVBAR() "nav-bar",
  /**
   * Constant reference to the ID of the menubar's toolbar.
   */
  get AREA_MENUBAR() "toolbar-menubar",
  /**
   * Constant reference to the ID of the tabstrip toolbar.
   */
  get AREA_TABSTRIP() "TabsToolbar",
  /**
   * Constant reference to the ID of the bookmarks toolbar.
   */
  get AREA_BOOKMARKS() "PersonalToolbar",
  /**
   * Constant reference to the ID of the addon-bar toolbar shim.
   * Do not use, this will be removed as soon as reasonably possible.
   * @deprecated
   */
  get AREA_ADDONBAR() "addon-bar",
  /**
   * Constant indicating the area is a menu panel.
   */
  get TYPE_MENU_PANEL() "menu-panel",
  /**
   * Constant indicating the area is a toolbar.
   */
  get TYPE_TOOLBAR() "toolbar",

  /**
   * Constant indicating a XUL-type provider.
   */
  get PROVIDER_XUL() "xul",
  /**
   * Constant indicating an API-type provider.
   */
  get PROVIDER_API() "api",
  /**
   * Constant indicating dynamic (special) widgets: spring, spacer, and separator.
   */
  get PROVIDER_SPECIAL() "special",

  /**
   * Constant indicating the widget is built-in
   */
  get SOURCE_BUILTIN() "builtin",
  /**
   * Constant indicating the widget is externally provided
   * (e.g. by add-ons or other items not part of the builtin widget set).
   */
  get SOURCE_EXTERNAL() "external",

  /**
   * The class used to distinguish items that span the entire menu panel.
   */
  get WIDE_PANEL_CLASS() "panel-wide-item",
  /**
   * The (constant) number of columns in the menu panel.
   */
  get PANEL_COLUMN_COUNT() 3,

  /**
   * Add a listener object that will get fired for various events regarding
   * customization.
   *
   * @param aListener the listener object to add
   *
   * Not all event handler methods need to be defined.
   * CustomizableUI will catch exceptions. Events are dispatched
   * synchronously on the UI thread, so if you can delay any/some of your
   * processing, that is advisable. The following event handlers are supported:
   *   - onWidgetAdded(aWidgetId, aArea, aPosition)
   *     Fired when a widget is added to an area. aWidgetId is the widget that
   *     was added, aArea the area it was added to, and aPosition the position
   *     in which it was added.
   *   - onWidgetMoved(aWidgetId, aArea, aOldPosition, aNewPosition)
   *     Fired when a widget is moved within its area. aWidgetId is the widget
   *     that was moved, aArea the area it was moved in, aOldPosition its old
   *     position, and aNewPosition its new position.
   *   - onWidgetRemoved(aWidgetId, aArea)
   *     Fired when a widget is removed from its area. aWidgetId is the widget
   *     that was removed, aArea the area it was removed from.
   *
   *   - onWidgetBeforeDOMChange(aNode, aNextNode, aContainer, aIsRemoval)
   *     Fired *before* a widget's DOM node is acted upon by CustomizableUI
   *     (to add, move or remove it). aNode is the DOM node changed, aNextNode
   *     the DOM node (if any) before which a widget will be inserted,
   *     aContainer the *actual* DOM container (could be an overflow panel in
   *     case of an overflowable toolbar), and aWasRemoval is true iff the
   *     action about to happen is the removal of the DOM node.
   *   - onWidgetAfterDOMChange(aNode, aNextNode, aContainer, aWasRemoval)
   *     Like onWidgetBeforeDOMChange, but fired after the change to the DOM
   *     node of the widget.
   *
   *   - onWidgetReset(aNode, aContainer)
   *     Fired after a reset to default placements moves a widget's node to a
   *     different location. aNode is the widget's node, aContainer is the
   *     area it was moved into (NB: it might already have been there and been
   *     moved to a different position!)
   *   - onAreaReset(aArea, aContainer)
   *     Fired after a reset to default placements is complete on an area's
   *     DOM node. Note that this is fired for each DOM node. aArea is the area
   *     that was reset, aContainer the DOM node that was reset.
   *
   *   - onWidgetCreated(aWidgetId)
   *     Fired when a widget with id aWidgetId has been created, but before it
   *     is added to any placements or any DOM nodes have been constructed.
   *     Only fired for API-based widgets.
   *   - onWidgetAfterCreation(aWidgetId, aArea)
   *     Fired after a widget with id aWidgetId has been created, and has been
   *     added to either its default area or the area in which it was placed
   *     previously. If the widget has no default area and/or it has never
   *     been placed anywhere, aArea may be null. Only fired for API-based
   *     widgets.
   *   - onWidgetDestroyed(aWidgetId)
   *     Fired when widgets are destroyed. aWidgetId is the widget that is
   *     being destroyed. Only fired for API-based widgets.
   *   - onWidgetInstanceRemoved(aWidgetId, aDocument)
   *     Fired when a window is unloaded and a widget's instance is destroyed
   *     because of this. Only fired for API-based widgets.
   *
   *   - onWidgetDrag(aWidgetId, aArea)
   *     Fired both when and after customize mode drag handling system tries
   *     to determine the width and height of widget aWidgetId when dragged to a
   *     different area. aArea will be the area the item is dragged to, or
   *     undefined after the measurements have been done and the node has been
   *     moved back to its 'regular' area.
   *
   *   - onCustomizeStart(aWindow)
   *     Fired when opening customize mode in aWindow.
   *   - onCustomizeEnd(aWindow)
   *     Fired when exiting customize mode in aWindow.
   *
   *   - onWidgetOverflow(aNode, aContainer)
   *     Fired when a widget's DOM node is overflowing its container, a toolbar,
   *     and will be displayed in the overflow panel.
   *   - onWidgetUnderflow(aNode, aContainer)
   *     Fired when a widget's DOM node is *not* overflowing its container, a
   *     toolbar, anymore.
   */
  addListener: function(aListener) {
    CustomizableUIInternal.addListener(aListener);
  },
  /**
   * Remove a listener added with addListener
   * @param aListener the listener object to remove
   */
  removeListener: function(aListener) {
    CustomizableUIInternal.removeListener(aListener);
  },

  /**
   * Register a customizable area with CustomizableUI.
   * @param aName   the name of the area to register. Can only contain
   *                alphanumeric characters, dashes (-) and underscores (_).
   * @param aProps  the properties of the area. The following properties are
   *                recognized:
   *                - type:   the type of area. Either TYPE_TOOLBAR (default) or
   *                          TYPE_MENU_PANEL;
   *                - anchor: for a menu panel or overflowable toolbar, the
   *                          anchoring node for the panel.
   *                - legacy: set to true if you want customizableui to
   *                          automatically migrate the currentset attribute
   *                - overflowable: set to true if your toolbar is overflowable.
   *                                This requires an anchor, and only has an
   *                                effect for toolbars.
   *                - defaultPlacements: an array of widget IDs making up the
   *                                     default contents of the area
   *                - defaultCollapsed: (INTERNAL ONLY) applies if the type is TYPE_TOOLBAR, specifies
   *                                    if toolbar is collapsed by default (default to true)
   */
  registerArea: function(aName, aProperties) {
    CustomizableUIInternal.registerArea(aName, aProperties);
  },
  /**
   * Register a concrete node for a registered area. This method is automatically
   * called from any toolbar in the main browser window that has its
   * "customizable" attribute set to true. There should normally be no need to
   * call it yourself.
   *
   * Note that ideally, you should register your toolbar using registerArea
   * before any of the toolbars have their XBL bindings constructed (which
   * will happen when they're added to the DOM and are not hidden). If you
   * don't, and your toolbar has a defaultset attribute, CustomizableUI will
   * register it automatically. If your toolbar does not have a defaultset
   * attribute, the node will be saved for processing when you call
   * registerArea. Note that CustomizableUI won't restore state in the area,
   * allow the user to customize it in customize mode, or otherwise deal
   * with it, until the area has been registered.
   */
  registerToolbarNode: function(aToolbar, aExistingChildren) {
    CustomizableUIInternal.registerToolbarNode(aToolbar, aExistingChildren);
  },
  /**
   * Register the menu panel node. This method should not be called by anyone
   * apart from the built-in PanelUI.
   * @param aPanel the panel DOM node being registered.
   */
  registerMenuPanel: function(aPanel) {
    CustomizableUIInternal.registerMenuPanel(aPanel);
  },
  /**
   * Unregister a customizable area. The inverse of registerArea.
   *
   * Unregistering an area will remove all the (removable) widgets in the
   * area, which will return to the panel, and destroy all other traces
   * of the area within CustomizableUI. Note that this means the *contents*
   * of the area's DOM nodes will be moved to the panel or removed, but
   * the area's DOM nodes *themselves* will stay.
   *
   * Furthermore, by default the placements of the area will be kept in the
   * saved state (!) and restored if you re-register the area at a later
   * point. This is useful for e.g. add-ons that get disabled and then
   * re-enabled (e.g. when they update).
   *
   * You can override this last behaviour (and destroy the placements
   * information in the saved state) by passing true for aDestroyPlacements.
   *
   * @param aName              the name of the area to unregister
   * @param aDestroyPlacements whether to destroy the placements information
   *                           for the area, too.
   */
  unregisterArea: function(aName, aDestroyPlacements) {
    CustomizableUIInternal.unregisterArea(aName, aDestroyPlacements);
  },
  /**
   * Add a widget to an area.
   * If the area to which you try to add is not known to CustomizableUI,
   * this will throw.
   * If the area to which you try to add has not yet been restored from its
   * legacy state, this will postpone the addition.
   * If the area to which you try to add is the same as the area in which
   * the widget is currently placed, this will do the same as
   * moveWidgetWithinArea.
   * If the widget cannot be removed from its original location, this will
   * no-op.
   *
   * This will fire an onWidgetAdded notification,
   * and an onWidgetBeforeDOMChange and onWidgetAfterDOMChange notification
   * for each window CustomizableUI knows about.
   *
   * @param aWidgetId the ID of the widget to add
   * @param aArea     the ID of the area to add the widget to
   * @param aPosition the position at which to add the widget. If you do not
   *                  pass a position, the widget will be added to the end
   *                  of the area.
   */
  addWidgetToArea: function(aWidgetId, aArea, aPosition) {
    CustomizableUIInternal.addWidgetToArea(aWidgetId, aArea, aPosition);
  },
  /**
   * Remove a widget from its area. If the widget cannot be removed from its
   * area, or is not in any area, this will no-op. Otherwise, this will fire an
   * onWidgetRemoved notification, and an onWidgetBeforeDOMChange and
   * onWidgetAfterDOMChange notification for each window CustomizableUI knows
   * about.
   *
   * @param aWidgetId the ID of the widget to remove
   */
  removeWidgetFromArea: function(aWidgetId) {
    CustomizableUIInternal.removeWidgetFromArea(aWidgetId);
  },
  /**
   * Move a widget within an area.
   * If the widget is not in any area, this will no-op.
   * If the widget is already at the indicated position, this will no-op.
   *
   * Otherwise, this will move the widget and fire an onWidgetMoved notification,
   * and an onWidgetBeforeDOMChange and onWidgetAfterDOMChange notification for
   * each window CustomizableUI knows about.
   *
   * @param aWidgetId the ID of the widget to move
   * @param aPosition the position to move the widget to.
   *                  Negative values or values greater than the number of
   *                  widgets will be interpreted to mean moving the widget to
   *                  respectively the first or last position.
   */
  moveWidgetWithinArea: function(aWidgetId, aPosition) {
    CustomizableUIInternal.moveWidgetWithinArea(aWidgetId, aPosition);
  },
  /**
   * Ensure a XUL-based widget created in a window after areas were
   * initialized moves to its correct position.
   * This is roughly equivalent to manually looking up the position and using
   * insertItem in the old API, but a lot less work for consumers.
   * Always prefer this over using toolbar.insertItem (which might no-op
   * because it delegates to addWidgetToArea) or, worse, moving items in the
   * DOM yourself.
   *
   * @param aWidgetId the ID of the widget that was just created
   * @param aWindow the window in which you want to ensure it was added.
   *
   * NB: why is this API per-window, you wonder? Because if you need this,
   * presumably you yourself need to create the widget in all the windows
   * and need to loop through them anyway.
   */
  ensureWidgetPlacedInWindow: function(aWidgetId, aWindow) {
    return CustomizableUIInternal.ensureWidgetPlacedInWindow(aWidgetId, aWindow);
  },
  /**
   * Start a batch update of items.
   * During a batch update, the customization state is not saved to the user's
   * preferences file, in order to reduce (possibly sync) IO.
   * Calls to begin/endBatchUpdate may be nested.
   *
   * Callers should ensure that NO MATTER WHAT they call endBatchUpdate once
   * for each call to beginBatchUpdate, even if there are exceptions in the
   * code in the batch update. Otherwise, for the duration of the
   * Firefox session, customization state is never saved. Typically, you
   * would do this using a try...finally block.
   */
  beginBatchUpdate: function() {
    CustomizableUIInternal.beginBatchUpdate();
  },
  /**
   * End a batch update. See the documentation for beginBatchUpdate above.
   *
   * State is not saved if we believe it is identical to the last known
   * saved state. State is only ever saved when all batch updates have
   * finished (ie there has been 1 endBatchUpdate call for each
   * beginBatchUpdate call). If any of the endBatchUpdate calls pass
   * aForceDirty=true, we will flush to the prefs file.
   *
   * @param aForceDirty force CustomizableUI to flush to the prefs file when
   *                    all batch updates have finished.
   */
  endBatchUpdate: function(aForceDirty) {
    CustomizableUIInternal.endBatchUpdate(aForceDirty);
  },
  /**
   * Create a widget.
   *
   * To create a widget, you should pass an object with its desired
   * properties. The following properties are supported:
   *
   * - id:            the ID of the widget (required).
   * - type:          a string indicating the type of widget. Possible types
   *                  are:
   *                  'button' - for simple button widgets (the default)
   *                  'view'   - for buttons that open a panel or subview,
   *                             depending on where they are placed.
   *                  'custom' - for fine-grained control over the creation
   *                             of the widget.
   * - viewId:        Only useful for views (and required there): the id of the
   *                  <panelview> that should be shown when clicking the widget.
   * - onBuild(aDoc): Only useful for custom widgets (and required there); a
   *                  function that will be invoked with the document in which
   *                  to build a widget. Should return the DOM node that has
   *                  been constructed.
   * - onCreated(aNode): Attached to all widgets; a function that will be invoked
   *                  whenever the widget has a DOM node constructed, passing the
   *                  constructed node as an argument.
   * - onCommand(aEvt): Only useful for button widgets; a function that will be
   *                    invoked when the user activates the button.
   * - onClick(aEvt): Attached to all widgets; a function that will be invoked
   *                  when the user clicks the widget.
   * - onViewShowing(aEvt): Only useful for views; a function that will be
   *                  invoked when a user shows your view.
   * - onViewHiding(aEvt): Only useful for views; a function that will be
   *                  invoked when a user hides your view.
   * - tooltiptext:   string to use for the tooltip of the widget
   * - label:         string to use for the label of the widget
   * - removable:     whether the widget is removable (optional, default: true)
   *                  NB: if you specify false here, you must provide a
   *                  defaultArea, too.
   * - overflows:     whether widget can overflow when in an overflowable
   *                  toolbar (optional, default: true)
   * - defaultArea:   default area to add the widget to
   *                  (optional, default: none; required if non-removable)
   * - shortcutId:    id of an element that has a shortcut for this widget
   *                  (optional, default: null). This is only used to display
   *                  the shortcut as part of the tooltip for builtin widgets
   *                  (which have strings inside
   *                  customizableWidgets.properties). If you're in an add-on,
   *                  you should not set this property.
   * - showInPrivateBrowsing: whether to show the widget in private browsing
   *                          mode (optional, default: true)
   *
   * @param aProperties the specifications for the widget.
   * @return a wrapper around the created widget (see getWidget)
   */
  createWidget: function(aProperties) {
    return CustomizableUIInternal.wrapWidget(
      CustomizableUIInternal.createWidget(aProperties)
    );
  },
  /**
   * Destroy a widget
   *
   * If the widget is part of the default placements in an area, this will
   * remove it from there. It will also remove any DOM instances. However,
   * it will keep the widget in the placements for whatever area it was
   * in at the time. You can remove it from there yourself by calling
   * CustomizableUI.removeWidgetFromArea(aWidgetId).
   *
   * @param aWidgetId the ID of the widget to destroy
   */
  destroyWidget: function(aWidgetId) {
    CustomizableUIInternal.destroyWidget(aWidgetId);
  },
  /**
   * Get a wrapper object with information about the widget.
   * The object provides the following properties
   * (all read-only unless otherwise indicated):
   *
   * - id:            the widget's ID;
   * - type:          the type of widget (button, view, custom). For
   *                  XUL-provided widgets, this is always 'custom';
   * - provider:      the provider type of the widget, id est one of
   *                  PROVIDER_API or PROVIDER_XUL;
   * - forWindow(w):  a method to obtain a single window wrapper for a widget,
   *                  in the window w passed as the only argument;
   * - instances:     an array of all instances (single window wrappers)
   *                  of the widget. This array is NOT live;
   * - areaType:      the type of the widget's current area
   * - isGroup:       true; will be false for wrappers around single widget nodes;
   * - source:        for API-provided widgets, whether they are built-in to
   *                  Firefox or add-on-provided;
   * - disabled:      for API-provided widgets, whether the widget is currently
   *                  disabled. NB: this property is writable, and will toggle
   *                  all the widgets' nodes' disabled states;
   * - label:         for API-provied widgets, the label of the widget;
   * - tooltiptext:   for API-provided widgets, the tooltip of the widget;
   * - showInPrivateBrowsing: for API-provided widgets, whether the widget is
   *                          visible in private browsing;
   *
   * Single window wrappers obtained through forWindow(someWindow) or from the
   * instances array have the following properties
   * (all read-only unless otherwise indicated):
   *
   * - id:            the widget's ID;
   * - type:          the type of widget (button, view, custom). For
   *                  XUL-provided widgets, this is always 'custom';
   * - provider:      the provider type of the widget, id est one of
   *                  PROVIDER_API or PROVIDER_XUL;
   * - node:          reference to the corresponding DOM node;
   * - anchor:        the anchor on which to anchor panels opened from this
   *                  node. This will point to the overflow chevron on
   *                  overflowable toolbars if and only if your widget node
   *                  is overflowed, to the anchor for the panel menu
   *                  if your widget is inside the panel menu, and to the
   *                  node itself in all other cases;
   * - overflowed:    boolean indicating whether the node is currently in the
   *                  overflow panel of the toolbar;
   * - isGroup:       false; will be true for the group widget;
   * - label:         for API-provided widgets, convenience getter for the
   *                  label attribute of the DOM node;
   * - tooltiptext:   for API-provided widgets, convenience getter for the
   *                  tooltiptext attribute of the DOM node;
   * - disabled:      for API-provided widgets, convenience getter *and setter*
   *                  for the disabled state of this single widget. Note that
   *                  you may prefer to use the group wrapper's getter/setter
   *                  instead.
   *
   * @param aWidgetId the ID of the widget whose information you need
   * @return a wrapper around the widget as described above, or null if the
   *         widget is known not to exist (anymore). NB: non-null return
   *         is no guarantee the widget exists because we cannot know in
   *         advance if a XUL widget exists or not.
   */
  getWidget: function(aWidgetId) {
    return CustomizableUIInternal.wrapWidget(aWidgetId);
  },
  /**
   * Get an array of widget wrappers (see getWidget) for all the widgets
   * which are currently not in any area (so which are in the palette).
   *
   * @param aWindowPalette the palette (and by extension, the window) in which
   *                       CustomizableUI should look. This matters because of
   *                       course XUL-provided widgets could be available in
   *                       some windows but not others, and likewise
   *                       API-provided widgets might not exist in a private
   *                       window (because of the showInPrivateBrowsing
   *                       property).
   *
   * @return an array of widget wrappers (see getWidget)
   */
  getUnusedWidgets: function(aWindowPalette) {
    return CustomizableUIInternal.getUnusedWidgets(aWindowPalette).map(
      CustomizableUIInternal.wrapWidget,
      CustomizableUIInternal
    );
  },
  /**
   * Get an array of all the widget IDs placed in an area. This is roughly
   * equivalent to fetching the currentset attribute and splitting by commas
   * in the legacy APIs. Modifying the array will not affect CustomizableUI.
   *
   * @param aArea the ID of the area whose placements you want to obtain.
   * @return an array containing the widget IDs that are in the area.
   *
   * NB: will throw if called too early (before placements have been fetched)
   *     or if the area is not currently known to CustomizableUI.
   */
  getWidgetIdsInArea: function(aArea) {
    if (!gAreas.has(aArea)) {
      throw new Error("Unknown customization area: " + aArea);
    }
    if (!gPlacements.has(aArea)) {
      throw new Error("Area not yet restored");
    }

    // We need to clone this, as we don't want to let consumers muck with placements
    return [...gPlacements.get(aArea)];
  },
  /**
   * Get an array of widget wrappers for all the widgets in an area. This is
   * the same as calling getWidgetIdsInArea and .map() ing the result through
   * CustomizableUI.getWidget. Careful: this means that if there are IDs in there
   * which don't have corresponding DOM nodes (like in the old-style currentset
   * attribute), there might be nulls in this array, or items for which
   * wrapper.forWindow(win) will return null.
   *
   * @param aArea the ID of the area whose widgets you want to obtain.
   * @return an array of widget wrappers and/or null values for the widget IDs
   *         placed in an area.
   *
   * NB: will throw if called too early (before placements have been fetched)
   *     or if the area is not currently known to CustomizableUI.
   */
  getWidgetsInArea: function(aArea) {
    return this.getWidgetIdsInArea(aArea).map(
      CustomizableUIInternal.wrapWidget,
      CustomizableUIInternal
    );
  },
  /**
   * Obtain an array of all the area IDs known to CustomizableUI.
   * This array is created for you, so is modifiable without CustomizableUI
   * being affected.
   */
  get areas() {
    return [area for ([area, props] of gAreas)];
  },
  /**
   * Check what kind of area (toolbar or menu panel) an area is. This is
   * useful if you have a widget that needs to behave differently depending
   * on its location. Note that widget wrappers have a convenience getter
   * property (areaType) for this purpose.
   *
   * @param aArea the ID of the area whose type you want to know
   * @return TYPE_TOOLBAR or TYPE_MENU_PANEL depending on the area, null if
   *         the area is unknown.
   */
  getAreaType: function(aArea) {
    let area = gAreas.get(aArea);
    return area ? area.get("type") : null;
  },
  /**
   * Check if a toolbar is collapsed by default.
   *
   * @param aArea the ID of the area whose default-collapsed state you want to know.
   * @return `true` or `false` depending on the area, null if the area is unknown.
   */
  isToolbarDefaultCollapsed: function(aArea) {
    let area = gAreas.get(aArea);
    return area ? area.get("defaultCollapsed") : null;
  },
  /**
   * Obtain the DOM node that is the customize target for an area in a
   * specific window.
   *
   * Areas can have a customization target that does not correspond to the
   * node itself. In particular, toolbars that have a customizationtarget
   * attribute set will have their customization target set to that node.
   * This means widgets will end up in the customization target, not in the
   * DOM node with the ID that corresponds to the area ID. This is useful
   * because it lets you have fixed content in a toolbar (e.g. the panel
   * menu item in the navbar) and have all the customizable widgets use
   * the customization target.
   *
   * Using this API yourself is discouraged; you should generally not need
   * to be asking for the DOM container node used for a particular area.
   * In particular, if you're wanting to check it in relation to a widget's
   * node, your DOM node might not be a direct child of the customize target
   * in a window if, for instance, the window is in customization mode, or if
   * this is an overflowable toolbar and the widget has been overflowed.
   *
   * @param aArea   the ID of the area whose customize target you want to have
   * @param aWindow the window where you want to fetch the DOM node.
   * @return the customize target DOM node for aArea in aWindow
   */
  getCustomizeTargetForArea: function(aArea, aWindow) {
    return CustomizableUIInternal.getCustomizeTargetForArea(aArea, aWindow);
  },
  /**
   * Reset the customization state back to its default.
   *
   * This is the nuclear option. You should never call this except if the user
   * explicitly requests it. Firefox does this when the user clicks the
   * "Restore Defaults" button in customize mode.
   */
  reset: function() {
    CustomizableUIInternal.reset();
  },
  /**
   * Get the placement of a widget. This is by far the best way to obtain
   * information about what the state of your widget is. The internals of
   * this call are cheap (no DOM necessary) and you will know where the user
   * has put your widget.
   *
   * @param aWidgetId the ID of the widget whose placement you want to know
   * @return
   *   {
   *     area: "somearea", // The ID of the area where the widget is placed
   *     position: 42 // the index in the placements array corresponding to
   *                  // your widget.
   *   }
   *
   *   OR
   *
   *   null // if the widget is not placed anywhere (ie in the palette)
   */
  getPlacementOfWidget: function(aWidgetId) {
    return CustomizableUIInternal.getPlacementOfWidget(aWidgetId, true);
  },
  /**
   * Check if a widget can be removed from the area it's in.
   *
   * Note that if you're wanting to move the widget somewhere, you should
   * generally be checking canWidgetMoveToArea, because that will return
   * true if the widget is already in the area where you want to move it (!).
   *
   * NB: oh, also, this method might lie if the widget in question is a
   *     XUL-provided widget and there are no windows open, because it
   *     can obviously not check anything in this case. It will return
   *     true. You will be able to move the widget elsewhere. However,
   *     once the user reopens a window, the widget will move back to its
   *     'proper' area automagically.
   *
   * @param aWidgetId a widget ID or DOM node to check
   * @return true if the widget can be removed from its area,
   *          false otherwise.
   */
  isWidgetRemovable: function(aWidgetId) {
    return CustomizableUIInternal.isWidgetRemovable(aWidgetId);
  },
  /**
   * Check if a widget can be moved to a particular area. Like
   * isWidgetRemovable but better, because it'll return true if the widget
   * is already in the right area.
   *
   * @param aWidgetId the widget ID or DOM node you want to move somewhere
   * @param aArea     the area ID you want to move it to.
   * @return true if this is possible, false if it is not. The same caveats as
   *              for isWidgetRemovable apply, however, if no windows are open.
   */
  canWidgetMoveToArea: function(aWidgetId, aArea) {
    return CustomizableUIInternal.canWidgetMoveToArea(aWidgetId, aArea);
  },
  /**
   * Whether we're in a default state. Note that non-removable non-default
   * widgets and non-existing widgets are not taken into account in determining
   * whether we're in the default state.
   *
   * NB: this is a property with a getter. The getter is NOT cheap, because
   * it does smart things with non-removable non-default items, non-existent
   * items, and so forth. Please don't call unless necessary.
   */
  get inDefaultState() {
    return CustomizableUIInternal.inDefaultState;
  },
  /**
   * Get a localized property off a (widget?) object.
   *
   * NB: this is unlikely to be useful unless you're in Firefox code, because
   *     this code uses the builtin widget stringbundle, and can't be told
   *     to use add-on-provided strings. It's mainly here as convenience for
   *     custom builtin widgets that build their own DOM but use the same
   *     stringbundle as the other builtin widgets.
   *
   * @param aWidget     the object whose property we should use to fetch a
   *                    localizable string;
   * @param aProp       the property on the object to use for the fetching;
   * @param aFormatArgs (optional) any extra arguments to use for a formatted
   *                    string;
   * @param aDef        (optional) the default to return if we don't find the
   *                    string in the stringbundle;
   *
   * @return the localized string, or aDef if the string isn't in the bundle.
   *         If no default is provided,
   *           if aProp exists on aWidget, we'll return that,
   *           otherwise we'll return the empty string
   *
   */
  getLocalizedProperty: function(aWidget, aProp, aFormatArgs, aDef) {
    return CustomizableUIInternal.getLocalizedProperty(aWidget, aProp,
      aFormatArgs, aDef);
  },
  /**
   * Given a node, walk up to the first panel in its ancestor chain, and
   * close it.
   *
   * @param aNode a node whose panel should be closed;
   */
  hidePanelForNode: function(aNode) {
    CustomizableUIInternal.hidePanelForNode(aNode);
  },
  /**
   * Check if a widget is a "special" widget: a spring, spacer or separator.
   *
   * @param aWidgetId the widget ID to check.
   * @return true if the widget is 'special', false otherwise.
   */
  isSpecialWidget: function(aWidgetId) {
    return CustomizableUIInternal.isSpecialWidget(aWidgetId);
  },
  /**
   * Add listeners to a panel that will close it. For use from the menu panel
   * and overflowable toolbar implementations, unlikely to be useful for
   * consumers.
   *
   * @param aPanel the panel to which listeners should be attached.
   */
  addPanelCloseListeners: function(aPanel) {
    CustomizableUIInternal.addPanelCloseListeners(aPanel);
  },
  /**
   * Remove close listeners that have been added to a panel with
   * addPanelCloseListeners. For use from the menu panel and overflowable
   * toolbar implementations, unlikely to be useful for consumers.
   *
   * @param aPanel the panel from which listeners should be removed.
   */
  removePanelCloseListeners: function(aPanel) {
    CustomizableUIInternal.removePanelCloseListeners(aPanel);
  },
  /**
   * Notify listeners a widget is about to be dragged to an area. For use from
   * Customize Mode only, do not use otherwise.
   *
   * @param aWidgetId the ID of the widget that is being dragged to an area.
   * @param aArea     the ID of the area to which the widget is being dragged.
   */
  onWidgetDrag: function(aWidgetId, aArea) {
    CustomizableUIInternal.notifyListeners("onWidgetDrag", aWidgetId, aArea);
  },
  /**
   * Notify listeners that a window is entering customize mode. For use from
   * Customize Mode only, do not use otherwise.
   * @param aWindow the window entering customize mode
   */
  notifyStartCustomizing: function(aWindow) {
    CustomizableUIInternal.notifyListeners("onCustomizeStart", aWindow);
  },
  /**
   * Notify listeners that a window is exiting customize mode. For use from
   * Customize Mode only, do not use otherwise.
   * @param aWindow the window exiting customize mode
   */
  notifyEndCustomizing: function(aWindow) {
    CustomizableUIInternal.notifyListeners("onCustomizeEnd", aWindow);
  },
  /**
   * Check whether an area is overflowable.
   *
   * @param aAreaId the ID of an area to check for overflowable-ness
   * @return true if the area is overflowable, false otherwise.
   */
  isAreaOverflowable: function(aAreaId) {
    let area = gAreas.get(aAreaId);
    return area ? area.get("type") == this.TYPE_TOOLBAR && area.get("overflowable")
                : false;
  },
  /**
   * Obtain a string indicating the place of an element. This is intended
   * for use from customize mode; You should generally use getPlacementOfWidget
   * instead, which is cheaper because it does not use the DOM.
   *
   * @param aElement the DOM node whose place we need to check
   * @return "toolbar" if the node is in a toolbar, "panel" if it is in the
   *         menu panel, "palette" if it is in the (visible!) customization
   *         palette, undefined otherwise.
   */
  getPlaceForItem: function(aElement) {
    let place;
    let node = aElement;
    while (node && !place) {
      if (node.localName == "toolbar")
        place = "toolbar";
      else if (node.id == CustomizableUI.AREA_PANEL)
        place = "panel";
      else if (node.id == "customization-palette")
        place = "palette";

      node = node.parentNode;
    }
    return place;
  }
};
Object.freeze(this.CustomizableUI);


/**
 * All external consumers of widgets are really interacting with these wrappers
 * which provide a common interface.
 */

/**
 * WidgetGroupWrapper is the common interface for interacting with an entire
 * widget group - AKA, all instances of a widget across a series of windows.
 * This particular wrapper is only used for widgets created via the provider
 * API.
 */
function WidgetGroupWrapper(aWidget) {
  this.isGroup = true;

  const kBareProps = ["id", "source", "type", "disabled", "label", "tooltiptext",
                      "showInPrivateBrowsing"];
  for (let prop of kBareProps) {
    let propertyName = prop;
    this.__defineGetter__(propertyName, function() aWidget[propertyName]);
  }

  this.__defineGetter__("provider", function() CustomizableUI.PROVIDER_API);

  this.__defineSetter__("disabled", function(aValue) {
    aValue = !!aValue;
    aWidget.disabled = aValue;
    for (let [,instance] of aWidget.instances) {
      instance.disabled = aValue;
    }
  });

  this.forWindow = function WidgetGroupWrapper_forWindow(aWindow) {
    let wrapperMap;
    if (!gSingleWrapperCache.has(aWindow)) {
      wrapperMap = new Map();
      gSingleWrapperCache.set(aWindow, wrapperMap);
    } else {
      wrapperMap = gSingleWrapperCache.get(aWindow);
    }
    if (wrapperMap.has(aWidget.id)) {
      return wrapperMap.get(aWidget.id);
    }

    let instance = aWidget.instances.get(aWindow.document);
    if (!instance &&
        (aWidget.showInPrivateBrowsing || !PrivateBrowsingUtils.isWindowPrivate(aWindow))) {
      instance = CustomizableUIInternal.buildWidget(aWindow.document,
                                                    aWidget);
    }

    let wrapper = new WidgetSingleWrapper(aWidget, instance);
    wrapperMap.set(aWidget.id, wrapper);
    return wrapper;
  };

  this.__defineGetter__("instances", function() {
    // Can't use gBuildWindows here because some areas load lazily:
    let placement = CustomizableUIInternal.getPlacementOfWidget(aWidget.id);
    if (!placement) {
      return [];
    }
    let area = placement.area;
    let buildAreas = gBuildAreas.get(area);
    if (!buildAreas) {
      return [];
    }
    return [this.forWindow(node.ownerDocument.defaultView) for (node of buildAreas)];
  });

  this.__defineGetter__("areaType", function() {
    let areaProps = gAreas.get(aWidget.currentArea);
    return areaProps && areaProps.get("type");
  });

  Object.freeze(this);
}

/**
 * A WidgetSingleWrapper is a wrapper around a single instance of a widget in
 * a particular window.
 */
function WidgetSingleWrapper(aWidget, aNode) {
  this.isGroup = false;

  this.node = aNode;
  this.provider = CustomizableUI.PROVIDER_API;

  const kGlobalProps = ["id", "type"];
  for (let prop of kGlobalProps) {
    this[prop] = aWidget[prop];
  }

  const nodeProps = ["label", "tooltiptext"];
  for (let prop of nodeProps) {
    let propertyName = prop;
    // Look at the node for these, instead of the widget data, to ensure the
    // wrapper always reflects this live instance.
    this.__defineGetter__(propertyName,
                          function() aNode.getAttribute(propertyName));
  }

  this.__defineGetter__("disabled", function() aNode.disabled);
  this.__defineSetter__("disabled", function(aValue) {
    aNode.disabled = !!aValue;
  });

  this.__defineGetter__("anchor", function() {
    let anchorId;
    // First check for an anchor for the area:
    let placement = CustomizableUIInternal.getPlacementOfWidget(aWidgetId);
    if (placement) {
      anchorId = gAreas.get(placement.area).get("anchor");
    }
    if (!anchorId) {
      anchorId = aNode.getAttribute("cui-anchorid");
    }

    return anchorId ? aNode.ownerDocument.getElementById(anchorId)
                    : aNode;
  });

  this.__defineGetter__("overflowed", function() {
    return aNode.classList.contains("overflowedItem");
  });

  Object.freeze(this);
}

/**
 * XULWidgetGroupWrapper is the common interface for interacting with an entire
 * widget group - AKA, all instances of a widget across a series of windows.
 * This particular wrapper is only used for widgets created via the old-school
 * XUL method (overlays, or programmatically injecting toolbaritems, or other
 * such things).
 */
//XXXunf Going to need to hook this up to some events to keep it all live.
function XULWidgetGroupWrapper(aWidgetId) {
  this.isGroup = true;
  this.id = aWidgetId;
  this.type = "custom";
  this.provider = CustomizableUI.PROVIDER_XUL;

  this.forWindow = function XULWidgetGroupWrapper_forWindow(aWindow) {
    let wrapperMap;
    if (!gSingleWrapperCache.has(aWindow)) {
      wrapperMap = new Map();
      gSingleWrapperCache.set(aWindow, wrapperMap);
    } else {
      wrapperMap = gSingleWrapperCache.get(aWindow);
    }
    if (wrapperMap.has(aWidgetId)) {
      return wrapperMap.get(aWidgetId);
    }

    let instance = aWindow.document.getElementById(aWidgetId);
    if (!instance) {
      // Toolbar palettes aren't part of the document, so elements in there
      // won't be found via document.getElementById().
      instance = aWindow.gNavToolbox.palette.getElementsByAttribute("id", aWidgetId)[0];
    }

    let wrapper = new XULWidgetSingleWrapper(aWidgetId, instance);
    wrapperMap.set(aWidgetId, wrapper);
    return wrapper;
  };

  this.__defineGetter__("areaType", function() {
    let placement = CustomizableUIInternal.getPlacementOfWidget(aWidgetId);
    if (!placement) {
      return null;
    }

    let areaProps = gAreas.get(placement.area);
    return areaProps && areaProps.get("type");
  });

  this.__defineGetter__("instances", function() {
    return [this.forWindow(win) for ([win,] of gBuildWindows)];
  });

  Object.freeze(this);
}

/**
 * A XULWidgetSingleWrapper is a wrapper around a single instance of a XUL 
 * widget in a particular window.
 */
function XULWidgetSingleWrapper(aWidgetId, aNode) {
  this.isGroup = false;

  this.id = aWidgetId;
  this.type = "custom";
  this.provider = CustomizableUI.PROVIDER_XUL;

  this.node = aNode;

  this.__defineGetter__("anchor", function() {
    let anchorId;
    // First check for an anchor for the area:
    let placement = CustomizableUIInternal.getPlacementOfWidget(aWidgetId);
    if (placement) {
      anchorId = gAreas.get(placement.area).get("anchor");
    }
    if (!anchorId) {
      anchorId = aNode.getAttribute("cui-anchorid");
    }

    return anchorId ? aNode.ownerDocument.getElementById(anchorId)
                    : aNode;
  });

  this.__defineGetter__("overflowed", function() {
    return aNode.classList.contains("overflowedItem");
  });

  Object.freeze(this);
}

const LAZY_RESIZE_INTERVAL_MS = 200;

function OverflowableToolbar(aToolbarNode) {
  this._toolbar = aToolbarNode;
  this._collapsed = new Map();
  this._enabled = true;

  this._toolbar.setAttribute("overflowable", "true");
  let doc = this._toolbar.ownerDocument;
  this._target = this._toolbar.customizationTarget;
  this._list = doc.getElementById(this._toolbar.getAttribute("overflowtarget"));
  this._list.toolbox = this._toolbar.toolbox;
  this._list.customizationTarget = this._list;

  Services.obs.addObserver(this, "browser-delayed-startup-finished", false);
}

OverflowableToolbar.prototype = {
  initialized: false,
  _forceOnOverflow: false,

  observe: function(aSubject, aTopic, aData) {
    if (aTopic == "browser-delayed-startup-finished" &&
        aSubject == this._toolbar.ownerDocument.defaultView) {
      Services.obs.removeObserver(this, "browser-delayed-startup-finished");
      this.init();
    }
  },

  init: function() {
    let doc = this._toolbar.ownerDocument;
    let window = doc.defaultView;
    window.addEventListener("resize", this);
    window.gNavToolbox.addEventListener("customizationstarting", this);
    window.gNavToolbox.addEventListener("aftercustomization", this);

    let chevronId = this._toolbar.getAttribute("overflowbutton");
    this._chevron = doc.getElementById(chevronId);
    this._chevron.addEventListener("command", this);

    this._panel = doc.getElementById("widget-overflow");
    this._panel.addEventListener("popuphiding", this);
    CustomizableUIInternal.addPanelCloseListeners(this._panel);

    CustomizableUI.addListener(this);

    // The 'overflow' event may have been fired before init was called.
    if (this._toolbar.overflowedDuringConstruction) {
      this.onOverflow(this._toolbar.overflowedDuringConstruction);
      this._toolbar.overflowedDuringConstruction = null;
    }

    this.initialized = true;
  },

  uninit: function() {
    this._toolbar.removeEventListener("overflow", this._toolbar);
    this._toolbar.removeEventListener("underflow", this._toolbar);
    this._toolbar.removeAttribute("overflowable");

    if (!this.initialized) {
      Services.obs.removeObserver(this, "browser-delayed-startup-finished");
      return;
    }

    this._disable();

    let window = this._toolbar.ownerDocument.defaultView;
    window.removeEventListener("resize", this);
    window.gNavToolbox.removeEventListener("customizationstarting", this);
    window.gNavToolbox.removeEventListener("aftercustomization", this);
    this._chevron.removeEventListener("command", this);
    this._panel.removeEventListener("popuphiding", this);
    CustomizableUI.removeListener(this);
    CustomizableUIInternal.removePanelCloseListeners(this._panel);
  },

  handleEvent: function(aEvent) {
    switch(aEvent.type) {
      case "resize":
        this._onResize(aEvent);
        break;
      case "command":
        this._onClickChevron(aEvent);
        break;
      case "popuphiding":
        this._onPanelHiding(aEvent);
        break;
      case "customizationstarting":
        this._disable();
        break;
      case "aftercustomization":
        this._enable();
        break;
    }
  },

  show: function() {
    let deferred = Promise.defer();
    if (this._panel.state == "open") {
      deferred.resolve();
      return deferred.promise;
    }
    let doc = this._panel.ownerDocument;
    this._panel.hidden = false;
    let anchor = doc.getAnonymousElementByAttribute(this._chevron, "class", "toolbarbutton-icon");
    this._panel.openPopup(anchor || this._chevron, "bottomcenter topright");
    this._chevron.open = true;

    this._panel.addEventListener("popupshown", function onPopupShown() {
      this.removeEventListener("popupshown", onPopupShown);
      deferred.resolve();
    });

    return deferred.promise;
  },

  _onClickChevron: function(aEvent) {
    if (this._chevron.open)
      this._panel.hidePopup();
    else {
      let doc = aEvent.target.ownerDocument;
      this._panel.hidden = false;
      let anchor = doc.getAnonymousElementByAttribute(this._chevron, "class", "toolbarbutton-icon");
      this._panel.openPopup(anchor || this._chevron, "bottomcenter topright");
    }
    this._chevron.open = !this._chevron.open;
  },

  _onPanelHiding: function(aEvent) {
    this._chevron.open = false;
  },

  onOverflow: function(aEvent) {
    if (!this._enabled ||
        (aEvent && aEvent.target != this._toolbar.customizationTarget))
      return;

    let child = this._target.lastChild;

    while (child && this._target.scrollLeftMax > 0) {
      let prevChild = child.previousSibling;

      if (child.getAttribute("overflows") != "false") {
        this._collapsed.set(child.id, this._target.clientWidth);
        child.classList.add("overflowedItem");
        child.setAttribute("cui-anchorid", this._chevron.id);
        CustomizableUIInternal.notifyListeners("onWidgetOverflow", child, this._target);

        this._list.insertBefore(child, this._list.firstChild);
        if (!this._toolbar.hasAttribute("overflowing")) {
          CustomizableUI.addListener(this);
        }
        this._toolbar.setAttribute("overflowing", "true");
      }
      child = prevChild;
    };

    let win = this._target.ownerDocument.defaultView;
    win.UpdateUrlbarSearchSplitterState();
  },

  _onResize: function(aEvent) {
    if (!this._lazyResizeHandler) {
      this._lazyResizeHandler = new DeferredTask(this._onLazyResize.bind(this),
                                                 LAZY_RESIZE_INTERVAL_MS);
    }
    this._lazyResizeHandler.arm();
  },

  _moveItemsBackToTheirOrigin: function(shouldMoveAllItems) {
    let placements = gPlacements.get(this._toolbar.id);
    while (this._list.firstChild) {
      let child = this._list.firstChild;
      let minSize = this._collapsed.get(child.id);

      if (!shouldMoveAllItems &&
          minSize &&
          this._target.clientWidth <= minSize) {
        return;
      }

      this._collapsed.delete(child.id);
      let beforeNodeIndex = placements.indexOf(child.id) + 1;
      // If this is a skipintoolbarset item, meaning it doesn't occur in the placements list,
      // we're inserting it at the end. This will mean first-in, first-out (more or less)
      // leading to as little change in order as possible.
      if (beforeNodeIndex == 0) {
        beforeNodeIndex = placements.length;
      }
      let inserted = false;
      for (; beforeNodeIndex < placements.length; beforeNodeIndex++) {
        let beforeNode = this._target.getElementsByAttribute("id", placements[beforeNodeIndex])[0];
        if (beforeNode) {
          this._target.insertBefore(child, beforeNode);
          inserted = true;
          break;
        }
      }
      if (!inserted) {
        this._target.appendChild(child);
      }
      child.removeAttribute("cui-anchorid");
      child.classList.remove("overflowedItem");
      CustomizableUIInternal.notifyListeners("onWidgetUnderflow", child, this._target);
    }

    let win = this._target.ownerDocument.defaultView;
    win.UpdateUrlbarSearchSplitterState();

    if (!this._collapsed.size) {
      this._toolbar.removeAttribute("overflowing");
      CustomizableUI.removeListener(this);
    }
  },

  _onLazyResize: function() {
    if (!this._enabled)
      return;

    this._moveItemsBackToTheirOrigin();
  },

  _disable: function() {
    this._enabled = false;
    this._moveItemsBackToTheirOrigin(true);
    if (this._lazyResizeHandler) {
      this._lazyResizeHandler.disarm();
    }
  },

  _enable: function() {
    this._enabled = true;
    this.onOverflow();
  },

  onWidgetBeforeDOMChange: function(aNode, aNextNode, aContainer) {
    if (aContainer != this._target && aContainer != this._list) {
      return;
    }
    // When we (re)move an item, update all the items that come after it in the list
    // with the minsize *of the item before the to-be-removed node*. This way, we
    // ensure that we try to move items back as soon as that's possible.
    if (aNode.parentNode == this._list) {
      let updatedMinSize;
      if (aNode.previousSibling) {
        updatedMinSize = this._collapsed.get(aNode.previousSibling.id);
      } else {
        // Force (these) items to try to flow back into the bar:
        updatedMinSize = 1;
      }
      let nextItem = aNode.nextSibling;
      while (nextItem) {
        this._collapsed.set(nextItem.id, updatedMinSize);
        nextItem = nextItem.nextSibling;
      }
    }
  },

  onWidgetAfterDOMChange: function(aNode, aNextNode, aContainer) {
    if (aContainer != this._target && aContainer != this._list) {
      return;
    }

    let nowInBar = aNode.parentNode == aContainer;
    let nowOverflowed = aNode.parentNode == this._list;
    let wasOverflowed = this._collapsed.has(aNode.id);

    // If this wasn't overflowed before...
    if (!wasOverflowed) {
      // ... but it is now, then we added to the overflow panel. Exciting stuff:
      if (nowOverflowed) {
        // NB: we're guaranteed that it has a previousSibling, because if it didn't,
        // we would have added it to the toolbar instead. See getOverflowedNextNode.
        let prevId = aNode.previousSibling.id;
        let minSize = this._collapsed.get(prevId);
        this._collapsed.set(aNode.id, minSize);
        aNode.setAttribute("cui-anchorid", this._chevron.id);
        aNode.classList.add("overflowedItem");
        CustomizableUIInternal.notifyListeners("onWidgetOverflow", aNode, this._target);
      }
      // If it is not overflowed and not in the toolbar, and was not overflowed
      // either, it moved out of the toolbar. That means there's now space in there!
      // Let's try to move stuff back:
      else if (!nowInBar) {
        this._moveItemsBackToTheirOrigin(true);
      }
      // If it's in the toolbar now, then we don't care. An overflow event may
      // fire afterwards; that's ok!
    }
    // If it used to be overflowed...
    else {
      // ... and isn't anymore, let's remove our bookkeeping:
      if (!nowOverflowed) {
        this._collapsed.delete(aNode.id);
        aNode.removeAttribute("cui-anchorid");
        aNode.classList.remove("overflowedItem");
        CustomizableUIInternal.notifyListeners("onWidgetUnderflow", aNode, this._target);

        if (!this._collapsed.size) {
          this._toolbar.removeAttribute("overflowing");
          CustomizableUI.removeListener(this);
        }
      }
      // but if it still is, it must have changed places. Bookkeep:
      else {
        if (aNode.previousSibling) {
          let prevId = aNode.previousSibling.id;
          let minSize = this._collapsed.get(prevId);
          this._collapsed.set(aNode.id, minSize);
        } else {
          // If it's now the first item in the overflow list,
          // maybe we can return it:
          this._moveItemsBackToTheirOrigin();
        }
      }
    }
  },

  getOverflowedInsertionPoints: function(aNode, aNextNodeId) {
    if (aNode.getAttribute("overflows") == "false") {
      return [this._target, null];
    }
    // Inserting at the end means we're in the overflow list by definition:
    if (!aNextNodeId) {
      return [this._list, null];
    }

    let nextNode = this._list.getElementsByAttribute("id", aNextNodeId)[0];
    // If this is the first item, we can actually just append the node
    // to the end of the toolbar.  If it results in an overflow event, we'll move
    // the new node to the overflow target.
    if (!nextNode.previousSibling) {
      return [this._target, null];
    }
    return [this._list, nextNode];
  },

  getContainerFor: function(aNode) {
    if (aNode.classList.contains("overflowedItem")) {
      return this._list;
    }
    return this._target;
  },
};

CustomizableUIInternal.initialize();
