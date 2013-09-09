/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

this.EXPORTED_SYMBOLS = ["CustomizableUI"];

const {classes: Cc, interfaces: Ci, utils: Cu, results: Cr} = Components;

Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource://gre/modules/XPCOMUtils.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "CustomizableWidgets",
  "resource:///modules/CustomizableWidgets.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "DeferredTask",
  "resource://gre/modules/DeferredTask.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "PrivateBrowsingUtils",
  "resource://gre/modules/PrivateBrowsingUtils.jsm");
XPCOMUtils.defineLazyGetter(this, "gWidgetsBundle", function() {
  const kUrl = "chrome://browser/locale/customizableui/customizableWidgets.properties";
  return Services.strings.createBundle(kUrl);
});
XPCOMUtils.defineLazyServiceGetter(this, "gELS",
  "@mozilla.org/eventlistenerservice;1", "nsIEventListenerService");

const kNSXUL = "http://www.mozilla.org/keymaster/gatekeeper/there.is.only.xul";

const kSpecialWidgetPfx = "customizableui-special-";

const kCustomizationContextMenu = "customizationContextMenu";


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
    ];
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
    });
    this.registerArea(CustomizableUI.AREA_NAVBAR, {
      legacy: true,
      anchor: "nav-bar-overflow-button",
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
        "social-toolbar-item",
      ]
    });
#ifndef XP_MACOSX
    this.registerArea(CustomizableUI.AREA_MENUBAR, {
      legacy: true,
      type: CustomizableUI.TYPE_TOOLBAR,
      defaultPlacements: [
        "menubar-items",
      ]
    });
#endif
    this.registerArea(CustomizableUI.AREA_TABSTRIP, {
      legacy: true,
      type: CustomizableUI.TYPE_TOOLBAR,
      defaultPlacements: [
        "tabbrowser-tabs",
        "new-tab-button",
        "alltabs-button",
        "tabs-closebutton",
      ]
    });
    this.registerArea(CustomizableUI.AREA_BOOKMARKS, {
      legacy: true,
      type: CustomizableUI.TYPE_TOOLBAR,
      defaultPlacements: [
        "personal-bookmarks",
      ]
    });

    this.registerArea(CustomizableUI.AREA_ADDONBAR, {
      type: CustomizableUI.TYPE_TOOLBAR,
      legacy: true,
      defaultPlacements: ["addonbar-closebutton", "status-bar"]
    });
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

  registerArea: function(aName, aProperties) {
    if (typeof aName != "string" || !/^[a-z0-9-_]{1,}$/i.test(aName)) {
      throw new Error("Invalid area name");
    }
    if (gAreas.has(aName)) {
      throw new Error("Area already registered");
    }

    let props = new Map();
    for (let key in aProperties) {
      //XXXgijs for special items, we need to make sure they have an appropriate ID
      // so we aren't perpetually in a non-default state:
      if (key == "defaultPlacements" && Array.isArray(aProperties[key])) {
        props.set(key, aProperties[key].map(x => this.isSpecialWidget(x) ? this.ensureSpecialWidgetId(x) : x ));
      } else {
        props.set(key, aProperties[key]);
      }
    }
    gAreas.set(aName, props);

    if (props.get("legacy")) {
      // Guarantee this area exists in gFuturePlacements, to avoid checking it in
      // various places elsewhere.
      gFuturePlacements.set(aName, new Set());
    } else {
      this.restoreStateForArea(aName);
    }
  },

  unregisterArea: function(aName) {
    if (typeof aName != "string" || !/^[a-z0-9-_]{1,}$/i.test(aName)) {
      throw new Error("Invalid area name");
    }
    if (!gAreas.has(aName)) {
      throw new Error("Area not registered");
    }

    // Move all the widgets out
    this.beginBatchUpdate();
    let placements = gPlacements.get(aName);
    placements.forEach(this.removeWidgetFromArea, this);

    // Delete all remaining traces.
    gAreas.delete(aName);
    gPlacements.delete(aName);
    gFuturePlacements.delete(aName);
    this.endBatchUpdate(true);
  },

  registerToolbar: function(aToolbar, aIsOverlayed) {
    let area = aToolbar.id;
    if (gBuildAreas.has(area) && gBuildAreas.get(area).has(aToolbar)) {
      return;
    }
    this.beginBatchUpdate();
    let document = aToolbar.ownerDocument;
    let areaProperties = gAreas.get(area);

    if (!areaProperties) {
      throw new Error("Unknown customization area: " + area);
    }

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

    // If the number of children of this toolbar exceeds the length of the
    // placements, then an add-on probably overlayed the toolbar, so we mark
    // it dirty.
    if (aIsOverlayed) {
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
    this.endBatchUpdate();
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

    this.beginBatchUpdate();

    let currentNode = container.firstChild;
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

      if (inPrivateWindow && provider == CustomizableUI.PROVIDER_API) {
        let widget = gPalette.get(id);
        if (!widget.showInPrivateBrowsing && inPrivateWindow) {
          continue;
        }
      }

      this.ensureButtonContextMenu(node, aArea == CustomizableUI.AREA_PANEL);
      if (node.localName == "toolbarbutton" && aArea == CustomizableUI.AREA_PANEL) {
        node.setAttribute("tabindex", "0");
        if (!node.hasAttribute("type")) {
          node.setAttribute("type", "wrap");
        }
      }

      this.insertWidgetBefore(node, currentNode, container, aArea);
      this._addParentFlex(node);
      if (gResetting)
        this.notifyListeners("onWidgetReset", id);
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
            if (palette) {
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

    this.endBatchUpdate();
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

  ensureButtonContextMenu: function(aNode, aShouldHaveCustomizationMenu) {
    let currentContextMenu = aNode.getAttribute("context") ||
                             aNode.getAttribute("contextmenu");
    if (aShouldHaveCustomizationMenu) {
      if (!currentContextMenu)
        aNode.setAttribute("context", kCustomizationContextMenu);
    } else {
      if (currentContextMenu == kCustomizationContextMenu)
        aNode.removeAttribute("context");
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
    return [];
  },

  registerMenuPanel: function(aPanel) {
    if (gBuildAreas.has(CustomizableUI.AREA_PANEL) &&
        gBuildAreas.get(CustomizableUI.AREA_PANEL).has(aPanel)) {
      return;
    }

    let document = aPanel.ownerDocument;

    for (let btn of aPanel.querySelectorAll("toolbarbutton")) {
      btn.setAttribute("tabindex", "0");
      this.ensureButtonContextMenu(btn, true);
      if (!btn.hasAttribute("type")) {
        btn.setAttribute("type", "wrap");
      }
    }

    aPanel.toolbox = document.getElementById("navigator-toolbox");
    aPanel.customizationTarget = aPanel;

    this.addPanelCloseListeners(aPanel);

    let placements = gPlacements.get(CustomizableUI.AREA_PANEL);
    this.buildArea(CustomizableUI.AREA_PANEL, placements, aPanel);
    this.registerBuildArea(CustomizableUI.AREA_PANEL, aPanel);
  },

  onWidgetAdded: function(aWidgetId, aArea, aPosition) {
    let areaNodes = gBuildAreas.get(aArea);
    if (!areaNodes) {
      return;
    }

    let placements = gPlacements.get(aArea);
    if (!placements) {
      ERROR("Could not find any placements for " + aArea +
            " when adding a widget.");
      return;
    }

    let area = gAreas.get(aArea);
    let showInPrivateBrowsing = gPalette.has(aWidgetId)
                              ? gPalette.get(aWidgetId).showInPrivateBrowsing
                              : true;
    let nextNodeId = placements[aPosition + 1];

    // Go through each of the nodes associated with this area and move the
    // widget to the requested location.
    for (let areaNode of areaNodes) {
      let window = areaNode.ownerDocument.defaultView;
      if (!showInPrivateBrowsing && PrivateBrowsingUtils.isWindowPrivate(window)) {
        continue;
      }

      let container = areaNode.customizationTarget;
      let [provider, widgetNode] = this.getWidgetNode(aWidgetId, window);

      this.ensureButtonContextMenu(widgetNode, aArea == CustomizableUI.AREA_PANEL);
      if (widgetNode.localName == "toolbarbutton" && aArea == CustomizableUI.AREA_PANEL) {
        widgetNode.setAttribute("tabindex", "0");
        if (!widgetNode.hasAttribute("type")) {
          widgetNode.setAttribute("type", "wrap");
        }
      }

      let nextNode = nextNodeId ? container.querySelector(idToSelector(nextNodeId))
                                : null;
      this.insertWidgetBefore(widgetNode, nextNode, container, aArea);
      this._addParentFlex(widgetNode);
      if (area.get("type") == CustomizableUI.TYPE_TOOLBAR) {
        areaNode.setAttribute("currentset", areaNode.currentSet);
      }
    }
  },

  onWidgetRemoved: function(aWidgetId, aArea) {
    let areaNodes = gBuildAreas.get(aArea);
    if (!areaNodes) {
      return;
    }

    let area = gAreas.get(aArea);
    let showInPrivateBrowsing = gPalette.has(aWidgetId)
                              ? gPalette.get(aWidgetId).showInPrivateBrowsing
                              : true;

    for (let areaNode of areaNodes) {
      let window = areaNode.ownerDocument.defaultView;
      if (!showInPrivateBrowsing &&
          PrivateBrowsingUtils.isWindowPrivate(window)) {
        continue;
      }

      let container = areaNode.customizationTarget;
      let widgetNode = container.ownerDocument.getElementById(aWidgetId);
      if (!widgetNode) {
        ERROR("Widget not found, unable to remove");
        continue;
      }

      this._removeParentFlex(widgetNode);

      if (gPalette.has(aWidgetId) || this.isSpecialWidget(aWidgetId)) {
        container.removeChild(widgetNode);
      } else {
        this.removeLocationAttributes(widgetNode);
        widgetNode.removeAttribute("tabindex");
        if (widgetNode.getAttribute("type") == "wrap") {
          widgetNode.removeAttribute("type");
        }
        areaNode.toolbox.palette.appendChild(widgetNode);
      }
      if (area.get("type") == CustomizableUI.TYPE_TOOLBAR) {
        areaNode.setAttribute("currentset", areaNode.currentSet);
      }

      let windowCache = gSingleWrapperCache.get(window);
      if (windowCache) {
        windowCache.delete(aWidgetId);
      }
    }
  },

  onWidgetMoved: function(aWidgetId, aArea, aOldPosition, aNewPosition) {
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

    let area = gAreas.get(aArea);
    let showInPrivateBrowsing = gPalette.has(aWidgetId)
                              ? gPalette.get(aWidgetId).showInPrivateBrowsing
                              : true;

    let nextNodeId = placements[aNewPosition + 1];

    for (let areaNode of areaNodes) {
      let window = areaNode.ownerDocument.defaultView;
      if (!showInPrivateBrowsing &&
          PrivateBrowsingUtils.isWindowPrivate(window)) {
        continue;
      }

      let container = areaNode.customizationTarget;
      let [provider, widgetNode] = this.getWidgetNode(aWidgetId, window);
      if (!widgetNode) {
        ERROR("Widget not found, unable to move");
        continue;
      }

      let nextNode = nextNodeId ? container.querySelector(idToSelector(nextNodeId))
                                : null;
      this.insertWidgetBefore(widgetNode, nextNode, container, aArea);
      if (area.get("type") == CustomizableUI.TYPE_TOOLBAR) {
        areaNode.setAttribute("currentset", areaNode.currentSet);
      }
    }
  },

  registerBuildArea: function(aArea, aNode) {
    // We ensure that the window is registered to have its customization data
    // cleaned up when unloading.
    let window = aNode.ownerDocument.defaultView;
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
    }

    aWindow.addEventListener("unload", this);
    aWindow.addEventListener("command", this, true);
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
  },

  setLocationAttributes: function(aNode, aArea) {
    let props = gAreas.get(aArea);
    if (!props) {
      throw new Error("Expected area " + aArea + " to have a properties Map " +
                      "associated with it.");
    }

    aNode.setAttribute("customizableui-areatype", props.get("type") || "");
    aNode.setAttribute("customizableui-anchorid", props.get("anchor") || "");
  },

  removeLocationAttributes: function(aNode) {
    aNode.removeAttribute("customizableui-areatype");
    aNode.removeAttribute("customizableui-anchorid");
  },

  insertWidgetBefore: function(aNode, aNextNode, aContainer, aArea) {
    this.setLocationAttributes(aNode, aArea);
    aContainer.insertBefore(aNode, aNextNode);
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
      // Due to timers resolution Date.now() can be the same for
      // elements created in small timeframes.  So ids are
      // differentiated through a unique count suffix.
      return kSpecialWidgetPfx + aId + Date.now() + (++gNewElementCount);
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
                         parent.localName == "toolbarpaletteitem")) {
        parent = parent.parentNode;
      }

      if ((parent && parent.customizationTarget == node.parentNode &&
           gBuildWindows.get(aWindow).has(parent.toolbox)) ||
          (parent && parent.localName == "toolbarpaletteitem")) {
        // Normalize the removable attribute. For backwards compat, if
        // the widget is not defined in a toolbox palette then absence
        // of the "removable" attribute means it is not removable.
        if (!node.hasAttribute("removable")) {
          // If we first see this in customization mode, it may be in the
          // customization palette instead of the toolbox palette.
          node.setAttribute("removable", !parent.customizationTarget);
        }

        return node;
      }
    }

    let toolboxes = gBuildWindows.get(aWindow);
    for (let toolbox of toolboxes) {
      if (toolbox.palette) {
        // Attempt to locate a node with a matching ID within
        // the palette.
        let node = toolbox.palette.querySelector(idToSelector(aId));
        if (node) {
          // Normalize the removable attribute. For backwards compat, this
          // is optional if the widget is defined in the toolbox palette,
          // and defaults to *true*, unlike if it was defined elsewhere.
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
      node.setAttribute("nooverflow", aWidget.nooverflow);
      node.setAttribute("label", this.getLocalizedProperty(aWidget, "label"));
      node.setAttribute("tooltiptext", this.getLocalizedProperty(aWidget, "tooltiptext"));
      //XXXunf Need to hook this up to a <key> element or something.
      let shortcut = this.getLocalizedProperty(aWidget, "shortcut", null, "none");
      if (shortcut != "none") {
        node.setAttribute("acceltext", shortcut);
      }
      node.setAttribute("class", "toolbarbutton-1 chromeclass-toolbar-additional");

      let commandHandler = this.handleWidgetCommand.bind(this, aWidget, node);
      node.addEventListener("command", commandHandler, false);
      let clickHandler = this.handleWidgetClick.bind(this, aWidget, node);
      node.addEventListener("click", clickHandler, false);

      // If the widget has a view, and has view showing / hiding listeners,
      // hook those up to this widget.
      if (aWidget.type == "view" &&
          (aWidget.onViewShowing || aWidget.onViewHiding)) {
        LOG("Widget " + aWidget.id + " has a view with showing and hiding events. Auto-registering event handlers.");
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
    if (typeof aWidget[aProp] == "string") {
      return aWidget[aProp];
    }
    let def = aDef || "";
    let name = aWidget.id + "." + aProp;
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
      this.maybeAutoHidePanel(aEvent);

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
    if (aWidget.type == "button") {
      this.maybeAutoHidePanel(aEvent);
    }

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
   * Anything else we should take care of?
   */
  _isOnInteractiveElement: function(aEvent) {
    let target = aEvent.originalTarget;
    let panel = aEvent.currentTarget;
    let inInput = false;
    let inMenu = false;
    while (!inInput && !inMenu && target != aEvent.currentTarget) {
      inInput = target.localName == "input";
      inMenu = target.type == "menu";
      target = target.parentNode;
    }
    return inMenu || inInput;
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
      // If consumers don't want this to happen, they should specify noautoclose.

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

    if (aEvent.target.getAttribute("noautoclose") == "true" ||
        aEvent.target.getAttribute("widget-type") == "view") {
      return;
    }

    // If we get here, we can actually hide the popup:
    this.hidePanelForNode(aEvent.target);
  },

  getUnusedWidgets: function(aWindowPalette) {
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
        widgets.add(id);
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

  getPlacementOfWidget: function(aWidgetId, aOnlyRegistered) {
    if (aOnlyRegistered && !this.widgetExists(aWidgetId)) {
      return null;
    }

    for (let [area, placements] of gPlacements) {
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

    let oldPlacement = this.getPlacementOfWidget(aWidgetId);
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
    let oldPlacement = this.getPlacementOfWidget(aWidgetId);
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
  },

  restoreStateForArea: function(aArea, aLegacyState) {
    if (gPlacements.has(aArea)) {
      // Already restored.
      return;
    }

    this.beginBatchUpdate();
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
    this.endBatchUpdate();
  },

  saveState: function() {
    if (gInBatchStack || !gDirty) {
      return;
    }
    let state = { placements: gPlacements,
                  seen: gSeenWidgets,
                  dirtyAreaCache: gDirtyAreaCache };

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
      if (autoAdd && !widget.currentArea && !gSeenWidgets.has(widget.id)) {
        this.beginBatchUpdate();
        gSeenWidgets.add(widget.id);

        if (widget.defaultArea) {
          if (this.isAreaLazy(widget.defaultArea)) {
            gFuturePlacements.get(widget.defaultArea).add(widget.id);
          } else {
            this.addWidgetToArea(widget.id, widget.defaultArea);
          }
        }

        this.endBatchUpdate(true);
      }
    }

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
      removable: false,
      nooverflow: false,
      defaultArea: null,
      allowedAreas: [],
      shortcut: null,
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

    const kOptStringProps = ["label", "tooltiptext", "shortcut"];
    for (let prop of kOptStringProps) {
      if (typeof aData[prop] == "string") {
        widget[prop] = aData[prop];
      }
    }

    const kOptBoolProps = ["removable", "showInPrivateBrowsing", "nooverflow"]
    for (let prop of kOptBoolProps) {
      if (typeof aData[prop] == "boolean") {
        widget[prop] = aData[prop];
      }
    }

    if (aData.defaultArea && gAreas.has(aData.defaultArea)) {
      widget.defaultArea = aData.defaultArea;
    }

    if (Array.isArray(aData.allowedAreas)) {
      widget.allowedAreas =
        [area for (area of aData.allowedAreas) if (gAreas.has(area))];
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

    let area = widget.currentArea;
    let buildAreaNodes = area && gBuildAreas.get(area);
    if (buildAreaNodes) {
      for (let buildNode of buildAreaNodes) {
        let widgetNode = buildNode.ownerDocument.getElementById(aWidgetId);
        let windowCache = gSingleWrapperCache.get(buildNode.ownerDocument.defaultView);
        if (windowCache) {
          windowCache.delete(aWidgetId);
        }
        if (widgetNode) {
          widgetNode.parentNode.removeChild(widgetNode);
        }
        if (widget.type == "view") {
          let viewNode = buildNode.ownerDocument.getElementById(widget.viewId);
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
    }

    gPalette.delete(aWidgetId);
    gGroupWrapperCache.delete(aWidgetId);

    this.notifyListeners("onWidgetDestroyed", aWidgetId);
  },

  getCustomizeTargetForArea: function(aArea, aWindow) {
    let buildAreaNodes = gBuildAreas.get(aArea);
    if (!buildAreaNodes) {
      throw new Error("No build area nodes registered for " + aArea);
    }

    for (let node of buildAreaNodes) {
      if (node.ownerDocument.defaultView === aWindow) {
        return node.customizationTarget ? node.customizationTarget : node;
      }
    }

    throw new Error("Could not find any window nodes for area " + aArea);
  },

  reset: function() {
    gResetting = true;
    Services.prefs.clearUserPref(kPrefCustomizationState);
    LOG("State reset");

    // Reset placements to make restoring default placements possible.
    gPlacements = new Map();
    gDirtyAreaCache = new Set();
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
      }
    }
    gResetting = false;
  },

  _addParentFlex: function(aElement) {
    // If necessary, add flex to accomodate new child.
    let elementFlex = aElement.getAttribute("flex");
    if (elementFlex) {
      let parent = aElement.parentNode;
      let parentFlex = +parent.getAttribute("flex") || 0;
      elementFlex = +elementFlex || 0;
      parent.setAttribute("flex", parentFlex + elementFlex);
    }
  },

  _removeParentFlex: function(aElement) {
    if (aElement.parentNode.hasAttribute("flex") && aElement.hasAttribute("flex")) {
      let parent = aElement.parentNode;
      let parentFlex = parseInt(parent.getAttribute("flex"), 10);
      let elementFlex = parseInt(aElement.getAttribute("flex"), 10);
      parent.setAttribute("flex", Math.max(0, parentFlex - elementFlex));
    }
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

  get inDefaultState() {
    for (let [areaId, props] of gAreas) {
      let defaultPlacements = props.get("defaultPlacements");
      // Areas without default placements (like legacy ones?) get skipped
      if (!defaultPlacements) {
        continue;
      }

      let currentPlacements = gPlacements.get(areaId);
      // We're excluding all of the placement IDs for items that do not exist,
      // because we don't want to consider them when determining if we're
      // in the default state. This way, if an add-on introduces a widget
      // and is then uninstalled, the leftover placement doesn't cause us to
      // automatically assume that the buttons are not in the default state.
      let buildAreaNodes = gBuildAreas.get(areaId);
      if (buildAreaNodes && buildAreaNodes.size) {
        let container = [...buildAreaNodes][0];
        // Clone the array so we don't modify the actual placements...
        currentPlacements = [...currentPlacements];
        // Loop backwards through the placements so we can easily remove items:
        let itemIndex = currentPlacements.length;
        while (itemIndex--) {
          if (!container.querySelector(idToSelector(currentPlacements[itemIndex]))) {
            currentPlacements.splice(itemIndex, 1);
          }
        }
      }
      LOG("Checking default state for " + areaId + ":\n" + currentPlacements.join("\n") +
          " vs. " + defaultPlacements.join("\n"));

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
  get AREA_PANEL() "PanelUI-contents",
  get AREA_NAVBAR() "nav-bar",
  get AREA_MENUBAR() "toolbar-menubar",
  get AREA_TABSTRIP() "TabsToolbar",
  get AREA_BOOKMARKS() "PersonalToolbar",
  get AREA_ADDONBAR() "addon-bar",

  get PROVIDER_XUL() "xul",
  get PROVIDER_API() "api",
  get PROVIDER_SPECIAL() "special",

  get SOURCE_BUILTIN() "builtin",
  get SOURCE_EXTERNAL() "external",

  get TYPE_BUTTON() "button",
  get TYPE_MENU_PANEL() "menu-panel",
  get TYPE_TOOLBAR() "toolbar",

  addListener: function(aListener) {
    CustomizableUIInternal.addListener(aListener);
  },
  removeListener: function(aListener) {
    CustomizableUIInternal.removeListener(aListener);
  },
  registerArea: function(aName, aProperties) {
    CustomizableUIInternal.registerArea(aName, aProperties);
  },
  //XXXunf registerToolbarNode / registerToolbarInstance ?
  registerToolbar: function(aToolbar) {
    CustomizableUIInternal.registerToolbar(aToolbar);
  },
  registerMenuPanel: function(aPanel) {
    CustomizableUIInternal.registerMenuPanel(aPanel);
  },
  unregisterArea: function(aName) {
    CustomizableUIInternal.unregisterArea(aName);
  },
  addWidgetToArea: function(aWidgetId, aArea, aPosition) {
    CustomizableUIInternal.addWidgetToArea(aWidgetId, aArea, aPosition);
  },
  removeWidgetFromArea: function(aWidgetId) {
    CustomizableUIInternal.removeWidgetFromArea(aWidgetId);
  },
  moveWidgetWithinArea: function(aWidgetId, aPosition) {
    CustomizableUIInternal.moveWidgetWithinArea(aWidgetId, aPosition);
  },
  beginBatchUpdate: function() {
    CustomizableUIInternal.beginBatchUpdate();
  },
  endBatchUpdate: function(aForceDirty) {
    CustomizableUIInternal.endBatchUpdate(aForceDirty);
  },
  createWidget: function(aProperties) {
    return CustomizableUIInternal.wrapWidget(
      CustomizableUIInternal.createWidget(aProperties)
    );
  },
  destroyWidget: function(aWidgetId) {
    CustomizableUIInternal.destroyWidget(aWidgetId);
  },
  getWidget: function(aWidgetId) {
    return CustomizableUIInternal.wrapWidget(aWidgetId);
  },
  getUnusedWidgets: function(aWindowPalette) {
    return CustomizableUIInternal.getUnusedWidgets(aWindowPalette).map(
      CustomizableUIInternal.wrapWidget,
      CustomizableUIInternal
    );
  },
  getWidgetIdsInArea: function(aArea) {
    if (!gAreas.has(aArea)) {
      throw new Error("Unknown customization area: " + aArea);
    }
    if (!gPlacements.has(aArea)) {
      throw new Error("Area not yet restored");
    }

    // We need to clone this, as we don't want to let consumers muck with placements
    return [].concat(gPlacements.get(aArea));
  },
  getWidgetsInArea: function(aArea) {
    return this.getWidgetIdsInArea(aArea).map(
      CustomizableUIInternal.wrapWidget,
      CustomizableUIInternal
    );
  },
  get areas() {
    return [area for ([area, props] of gAreas)];
  },
  getAreaType: function(aArea) {
    let area = gAreas.get(aArea);
    return area ? area.get("type") : null;
  },
  getCustomizeTargetForArea: function(aArea, aWindow) {
    return CustomizableUIInternal.getCustomizeTargetForArea(aArea, aWindow);
  },
  reset: function() {
    CustomizableUIInternal.reset();
  },
  getPlacementOfWidget: function(aWidgetId) {
    return CustomizableUIInternal.getPlacementOfWidget(aWidgetId, true);
  },
  isWidgetRemovable: function(aWidgetId) {
    return CustomizableUIInternal.isWidgetRemovable(aWidgetId);
  },
  canWidgetMoveToArea: function(aWidgetId, aArea) {
    return CustomizableUIInternal.canWidgetMoveToArea(aWidgetId, aArea);
  },
  get inDefaultState() {
    return CustomizableUIInternal.inDefaultState;
  },
  getLocalizedProperty: function(aWidget, aProp, aFormatArgs, aDef) {
    return CustomizableUIInternal.getLocalizedProperty(aWidget, aProp,
      aFormatArgs, aDef);
  },
  hidePanelForNode: function(aNode) {
    CustomizableUIInternal.hidePanelForNode(aNode);
  },
  isSpecialWidget: function(aWidgetId) {
    return CustomizableUIInternal.isSpecialWidget(aWidgetId);
  },
  addPanelCloseListeners: function(aPanel) {
    CustomizableUIInternal.addPanelCloseListeners(aPanel);
  },
  removePanelCloseListeners: function(aPanel) {
    CustomizableUIInternal.removePanelCloseListeners(aPanel);
  },
  onWidgetDrag: function(aWidgetId, aArea) {
    CustomizableUIInternal.notifyListeners("onWidgetDrag", aWidgetId, aArea);
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
    if (!instance) {
      instance = CustomizableUIInternal.buildWidget(aWindow.document,
                                                    aWidget);
    }

    let wrapper = new WidgetSingleWrapper(aWidget, instance);
    wrapperMap.set(aWidget.id, wrapper);
    return wrapper;
  };

  this.__defineGetter__("areaType", function() {
    return gAreas.get(aWidget.currentArea).get("type");
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
    let anchorId = aNode.getAttribute("customizableui-anchorid");
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
      instance = aWindow.gNavToolbox.palette.querySelector(idToSelector(aWidgetId));
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

    return gAreas.get(placement.area).get("type");
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
    let anchorId = aNode.getAttribute("customizableui-anchorid");
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
  this._collapsed = [];
  this._enabled = true;

  this._toolbar.customizationTarget.addEventListener("overflow", this);
  this._toolbar.setAttribute("overflowable", "true");
  Services.obs.addObserver(this, "browser-delayed-startup-finished", false);
}

OverflowableToolbar.prototype = {
  _initialized: false,
  _forceOnOverflow: false,

  observe: function(aSubject, aTopic, aData) {
    if (aTopic == "browser-delayed-startup-finished" &&
        aSubject == this._toolbar.ownerDocument.defaultView) {
      Services.obs.removeObserver(this, "browser-delayed-startup-finished");
      this.init();
    }
  },

  init: function() {
    this._target = this._toolbar.customizationTarget;
    let doc = this._toolbar.ownerDocument;
    this._list = doc.getElementById(this._toolbar.getAttribute("overflowtarget"));

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

    this._initialized = true;

    // The 'overflow' event may have been fired before init was called.
    if (this._forceOnOverflow) {
      this._onOverflow();
    }
  },

  uninit: function() {
    this._toolbar.customizationTarget.removeEventListener("overflow", this);
    this._toolbar.removeAttribute("overflowable");

    if (!this._initialized) {
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
    CustomizableUIInternal.removePanelCloseListeners(this._panel);
  },

  handleEvent: function(aEvent) {
    switch(aEvent.type) {
      case "overflow":
        if (this._initialized) {
          this._onOverflow();
        } else {
          this._forceOnOverflow = true;
        }
        break;
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

  _onOverflow: function() {
    if (!this._enabled)
      return;

    let child = this._target.lastChild;

    while(child && this._target.clientWidth < this._target.scrollWidth) {
      let prevChild = child.previousSibling;

      if (!child.hasAttribute("nooverflow")) {
        this._collapsed.push({child: child, minSize: this._target.clientWidth});
        child.classList.add("overflowedItem");
        child.setAttribute("customizableui-anchorid", this._chevron.id);

        this._list.insertBefore(child, this._list.firstChild);
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
    this._lazyResizeHandler.start();
  },

  _moveItemsBackToTheirOrigin: function(shouldMoveAllItems) {
    for (let i = this._collapsed.length - 1; i >= 0; i--) {
      let {child, minSize} = this._collapsed[i];

      if (!shouldMoveAllItems &&
          this._target.clientWidth <= minSize) {
        return;
      }

      this._collapsed.pop();
      this._target.appendChild(child);
      child.removeAttribute("customizableui-anchorid");
      child.classList.remove("overflowedItem");
    }

    let win = this._target.ownerDocument.defaultView;
    win.UpdateUrlbarSearchSplitterState();

    if (!this._collapsed.length) {
      this._toolbar.removeAttribute("overflowing");
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
      this._lazyResizeHandler.cancel();
    }
  },

  _enable: function() {
    this._enabled = true;
    this._onOverflow();
  }
};

// When IDs contain special characters, we need to escape them for use with querySelector:
function idToSelector(aId) {
  return "#" + aId.replace(/[ !"'#$%&\(\)*+\-,.\/:;<=>?@\[\\\]^`{|}~]/g, '\\$&');
}

CustomizableUIInternal.initialize();
