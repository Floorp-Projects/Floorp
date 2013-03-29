/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

this.EXPORTED_SYMBOLS = ["CustomizableUI"];

const {classes: Cc, interfaces: Ci, utils: Cu, results: Cr} = Components;

Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource://gre/modules/NetUtil.jsm");
Cu.import("resource://gre/modules/XPCOMUtils.jsm");

const kNSXUL = "http://www.mozilla.org/keymaster/gatekeeper/there.is.only.xul";

const kPrefCustomizationState        = "browser.uiCustomization.state";
const kPrefCustomizationAutoAdd      = "browser.uiCustomization.autoAdd";
const kPrefCustomizationDebug        = "browser.uiCustomization.debug";

/**
 * Lazily returns the definitions for the set of built-in widgets. This might
 * not be the final resting place for this stuff, but it'll do for now.
 */
XPCOMUtils.defineLazyGetter(this, "gBuiltInWidgets", function() {
  return [{
    id: "bookmarks-panelmenu",
    type: "view",
    viewId: "PanelUI-bookmarks",
    name: "Bookmarks...",
    description: "Bookmarks, yo!",
    defaultArea: CustomizableUI.AREA_PANEL,
    //XXXunf Need to enforce this, and make it optional.
    allowedAreas: [CustomizableUI.AREA_PANEL],
    icons: {
      "16": "chrome://branding/content/icon16.png",
      "32": "chrome://branding/content/icon48.png",
      "48": "chrome://branding/content/icon48.png"
    },
    // These are automatically bound to the ViewShowing and ViewHiding events
    // that the Panel will dispatch, targetted at the view referenced by
    // viewId.
    // XXXmconley: Is this too magical?
    onViewShowing: function(aEvent) {
      // Populate our list of bookmarks
      // Right now, I just jam in a tall vbox to simulate making the view
      // tall before switching to it.
      LOG("Bookmark view is being shown!");
      let doc = aEvent.detail.ownerDocument;
      let vbox = doc.getElementById("PanelUI-bookmarks-tall-maker");
      vbox.style.height = "500px";
    },
    onViewHiding: function(aEvent) {
      LOG("Bookmark view is being hidden!");
    }
  }, {
    id: "weather-indicator",
    name: "Weather",
    description: "Don't look out the window, look at your browser!",
    defaultArea: CustomizableUI.AREA_PANEL,
    //XXXunf Need to enforce this, and make it optional.
    allowedAreas: [CustomizableUI.AREA_PANEL],
    icons: {
      "16": "chrome://branding/content/icon16.png",
      "32": "chrome://branding/content/icon48.png",
      "48": "chrome://branding/content/icon48.png"
    }
  }, {
    id: "share-page",
    name: "Share 1",
    shortcut: "Ctrl+Alt+S",
    description: "Share this page",
    defaultArea: CustomizableUI.AREA_NAVBAR,
    allowedAreas: [CustomizableUI.AREA_PANEL, CustomizableUI.AREA_NAVBAR],
    icons: {
      "16": "chrome://branding/content/icon16.png",
      "32": "chrome://branding/content/icon48.png",
      "48": "chrome://branding/content/icon48.png"
    }
  }, {
    id: "share-page-2",
    name: "Share 2",
    shortcut: "Ctrl+Alt+S",
    description: "Share this page",
    defaultArea: CustomizableUI.AREA_PANEL,
    allowedAreas: [CustomizableUI.AREA_PANEL],
    icons: {
      "16": "chrome://branding/content/icon16.png",
      "32": "chrome://branding/content/icon48.png",
      "48": "chrome://branding/content/icon48.png"
    }
  }, {
    id: "share-page-3",
    name: "Share 3",
    shortcut: "Ctrl+Alt+S",
    description: "Share this page",
    allowedAreas: [CustomizableUI.AREA_PANEL],
    icons: {
      "16": "chrome://branding/content/icon16.png",
      "32": "chrome://branding/content/icon48.png",
      "48": "chrome://branding/content/icon48.png"
    }
  }, {
    id: "share-page-4",
    name: "Share 4",
    shortcut: "Ctrl+Alt+S",
    description: "Share this page",
    defaultArea: CustomizableUI.AREA_PANEL,
    allowedAreas: [CustomizableUI.AREA_PANEL],
    icons: {
      "16": "chrome://branding/content/icon16.png",
      "32": "chrome://branding/content/icon48.png",
      "48": "chrome://branding/content/icon48.png"
    }
  }, {
    id: "share-page-5",
    name: "Share 5",
    shortcut: "Ctrl+Alt+S",
    description: "Share this page",
    defaultArea: CustomizableUI.AREA_PANEL,
    allowedAreas: [CustomizableUI.AREA_PANEL],
    icons: {
      "16": "chrome://branding/content/icon16.png",
      "32": "chrome://branding/content/icon48.png",
      "48": "chrome://branding/content/icon48.png"
    }
  }];
});

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

/**
 * If the user does not have any state saved, this is the default set of
 * placements to use.
 */
let gDefaultPlacements = new Map([
  ["nav-bar", [
    "search-container",
    "bookmarks-menu-button-container",
    "downloads-button",
    "social-toolbar-button",
    "PanelUI-button",
    "share-page"
  ]],
  ["PanelUI-contents", [
    "new-window-button",
    "print-button",
    "history-button",
    "fullscreen-button",
    "share-page-5",
    "bookmarks-panelmenu",
  ]]
]);

//XXXunf Temporary. Need a nice way to abstract functions to build widgets
//       of these types.
let gSupportedWidgetTypes = new Set(["button", "view"]);

/**
 * gSeenWidgets remembers which widgets the user has seen for the first time
 * before. This way, if a new widget is created, and the user has not seen it
 * before, it can be put in its default location. Otherwise, it remains in the
 * palette.
 */
let gSeenWidgets = new Set();

let gSavedState = null;
let gRestoring = false;
let gDirty = false;
let gInBatch = false;

/**
 * gBuildAreas maps area IDs to actual area nodes within browser windows.
 */
let gBuildAreas = new Map();
let gNewElementCount = 0;
let gWrapperCache = new WeakMap();
let gListeners = new Set();

let gDebug = false;
try {
  gDebug = Services.prefs.getBoolPref(kPrefCustomizationDebug);
} catch (e) {}

function LOG(aMsg) {
  if (gDebug) {
    Services.console.logStringMessage("[CustomizableUI] " + aMsg);
  }
}
function ERROR(aMsg) Cu.reportError("[CustomizableUI] " + aMsg);


let CustomizableUIInternal = {
  initialize: function() {
    LOG("Initializing");

    this.addListener(this);
    this._defineBuiltInWidgets();
    this.loadSavedState();

    this.registerArea(CustomizableUI.AREA_PANEL);
    this.registerArea(CustomizableUI.AREA_NAVBAR, ["legacy"]);
  },

  _defineBuiltInWidgets: function() {
    //XXXunf Need to figure out how to auto-add new builtin widgets in new
    //       app versions to already customized areas.
    for (let widgetDefinition of gBuiltInWidgets) {
      this.createBuiltinWidget(widgetDefinition);
    }
  },

  wrapWidget: function(aWidgetId) {
    let provider = this.getWidgetProvider(aWidgetId);
    if (!provider) {
      return null;
    }

    if (provider == CustomizableUI.PROVIDER_API) {
      let widget = gPalette.get(aWidgetId);
      if (!widget.wrapper) {
        widget.wrapper = new WidgetGroupWrapper(widget);
      }
      return widget.wrapper;
    }

    // PROVIDER_SPECIAL gets treated the same as PROVIDER_XUL.
    return new XULWidgetGroupWrapper(aWidgetId);
  },

  registerArea: function(aName, aProperties) {
    if (typeof aName != "string" || !/^[a-z0-9-_]{1,}$/i.test(aName)) {
      throw new Error("Invalid area name");
    }
    if (gAreas.has(aName)) {
      throw new Error("Area already registered");
    }

    let props = new Set(aProperties);
    gAreas.set(aName, props);

    if (props.has("legacy")) {
      // Guarantee this area exists in gFuturePlacements, to avoid checking it in
      // various places elsewhere.
      gFuturePlacements.set(aName, new Set());
    } else {
      this.restoreStateForArea(aName);
    }
  },

  registerToolbar: function(aToolbar) {
    let document = aToolbar.ownerDocument;
    let area = aToolbar.id;

    if (!gAreas.has(area)) {
      throw new Error("Unknown customization area");
    }

    if (this.isBuildAreaRegistered(area, aToolbar)) {
      return;
    }

    let areaProperties = gAreas.get(area);

    if (!gPlacements.has(area) && areaProperties.has("legacy")) {
      let legacyState = aToolbar.getAttribute("currentset");
      if (legacyState) {
        legacyState = legacyState.split(",");
      }
      //XXXunf should the legacy attribute be purged?
      //       kinda messes up switching to older builds
      //XXXmconley No, I don't think so - I think we want to make it easy to
      //           switch back and forth between builds until this thing hits
      //           release.
      //aToolbar.removeAttribute("currentset");

      // Manually restore the state here, so the legacy state can be converted. 
      this.restoreStateForArea(area, legacyState);
    }

    this.registerBuildArea(area, aToolbar);

    let placements = gPlacements.get(area);
    this.buildArea(area, placements, aToolbar);
  },

  buildArea: function(aArea, aPlacements, aAreaNode) {
    let document = aAreaNode.ownerDocument;
    let container = aAreaNode.customizationTarget;

    if (!container) {
      throw new Error("Expected area " + aArea
                      + " to have a customizationTarget attribute.");
    }

    let currentNode = container.firstChild;
    for (let id of aPlacements) {
      if (currentNode && currentNode.id == id) {
        currentNode = currentNode.nextSibling;
        continue;
      }

      let [provider, node] = this.getWidgetNode(id, document, aAreaNode.toolbox);
      if (!node) {
        LOG("Unknown widget: " + id);
        continue;
      }

      if (provider == CustomizableUI.PROVIDER_XUL &&
          aArea == CustomizableUI.AREA_PANEL) {
        this.ensureButtonClosesPanel(node);
      }

      container.insertBefore(node, currentNode);
    }

    if (currentNode) {
      let palette = aAreaNode.toolbox ? aAreaNode.toolbox.palette : null;
      let limit = currentNode.previousSibling;
      let node = container.lastChild;
      while (node != limit) {
        // XXXunf Deprecating the old "removable" attribute, is this right?
        // XXXmconley I think we need to hear from UX about this.
        if (palette) {
          palette.appendChild(node);
        } else {
          container.removeChild(node);
        }
        node = container.lastChild;
      }
    }
  },

  ensureButtonClosesPanel: function(aNode) {
    //XXXunf This seems kinda hacky, but I can't find any other reliable method.
    //XXXunf Is it worth going to the effort to remove these when appropriate?
    // The "command" event only gets fired if there's not an attached command
    // (via the "command" attribute). For menus, this is handled natively, but
    // since the panel isn't implemented as a menu, we have to handle it
    // ourselves.

    aNode.addEventListener("mouseup", this.maybeAutoHidePanel, false);
    aNode.addEventListener("keypress", this.maybeAutoHidePanel, false);
  },

  getWidgetProvider: function(aWidgetId) {
    if (this.isSpecialWidget(aWidgetId)) {
      return CustomizableUI.PROVIDER_SPECIAL;
    }
    if (gPalette.has(aWidgetId)) {
      return CustomizableUI.PROVIDER_API;
    }

    // We fall back to the XUL provider, but we don't know for sure (at this
    // point) whether it exists there either. So the API is technically lying.
    // Ideally, it would be able to return an error value (or throw an
    // exception) if it really didn't exist. Our code calling this function
    // handles that fine, but this is a public API.
    return CustomizableUI.PROVIDER_XUL;
  },

  getWidgetNode: function(aWidgetId, aDocument, aToolbox) {
    if (this.isSpecialWidget(aWidgetId)) {
      return [ CustomizableUI.PROVIDER_SPECIAL,
               this.createSpecialWidget(aWidgetId, aDocument) ];
    }

    let widget = gPalette.get(aWidgetId);
    if (widget) {
      // If we have an instance of this widget already, just use that.
      if (widget.instances.has(aDocument)) {
        LOG("An instance of widget " + aWidgetId + " already exists in this "
            + "document. Reusing.");
        return [ CustomizableUI.PROVIDER_API,
                 widget.instances.get(aDocument) ];
      }

      return [ CustomizableUI.PROVIDER_API,
               this.buildWidget(aDocument, null, widget) ];
    }

    let node = this.findWidgetInToolbox(aWidgetId, aToolbox, aDocument);
    if (node) {
      return [ CustomizableUI.PROVIDER_XUL, node ];
    }

    return [];
  },

  registerMenuPanel: function(aPanel) {
    if (this.isBuildAreaRegistered(CustomizableUI.AREA_PANEL, aPanel)) {
      return;
    }

    let document = aPanel.ownerDocument;

    for (let btn of aPanel.querySelectorAll("toolbarbutton")) {
      if (!btn.hasAttribute("noautoclose")) {
        this.ensureButtonClosesPanel(btn);
      }
    }

    aPanel.toolbox = document.getElementById("navigator-toolbox");
    aPanel.customizationTarget = aPanel;

    let placements = gPlacements.get(CustomizableUI.AREA_PANEL);
    this.buildArea(CustomizableUI.AREA_PANEL, placements, aPanel);
    this.registerBuildArea(CustomizableUI.AREA_PANEL, aPanel);
  },

  registerWindow: function(aWindow) {
    aWindow.addEventListener("unload", this, false);
  },

  onWidgetAdded: function(aWidgetId, aArea, aPosition) {
    let areaNodes = gBuildAreas.get(aArea);
    if (!areaNodes) {
      return;
    }

    // Go through each of the nodes associated with this area and move the
    // widget to the requested location.
    for (let areaNode of areaNodes) {
      let container = areaNode.customizationTarget;
      let [provider, widgetNode] = this.getWidgetNode(aWidgetId,
                                                      container.ownerDocument,
                                                      areaNode.toolbox);

      if (provider == CustomizableUI.PROVIDER_XUL &&
          aArea == CustomizableUI.AREA_PANEL) {
        this.ensureButtonClosesPanel(widgetNode);
      }

      let nextNode = container.children.item(aPosition);
      container.insertBefore(widgetNode, nextNode);
    }
  },

  onWidgetRemoved: function(aWidgetId, aArea) {
    let areaNodes = gBuildAreas.get(aArea);
    if (!areaNodes) {
      return;
    }

    for (let areaNode of areaNodes) {
      let container = areaNode.customizationTarget;
      let widgetNode = container.ownerDocument.getElementById(aWidgetId);
      if (!widgetNode) {
        ERROR("Widget not found, unable to remove");
        continue;
      }

      if (gPalette.has(aWidgetId) || this.isSpecialWidget(aWidgetId)) {
        container.removeChild(widgetNode);
      } else {
        areaNode.toolbox.palette.appendChild(widgetNode);
      }
    }
  },

  onWidgetMoved: function(aWidgetId, aArea, aOldPosition, aNewPosition) {
    let areaNodes = gBuildAreas.get(aArea);
    if (!areaNodes) {
      return;
    }

    for (let areaNode of areaNodes) {
      let container = areaNode.customizationTarget;
      let widgetNode = container.ownerDocument.getElementById(aWidgetId);
      if (!widgetNode) {
        ERROR("Widget not found, unable to move");
        continue;
      }

      let nextNode = container.children.item(aNewPosition > aOldPosition ?
                                             aNewPosition + 1 :
                                             aNewPosition);
      container.insertBefore(widgetNode, nextNode);
    }
  },

  isBuildAreaRegistered: function(aArea, aInstance) {
    if (!gBuildAreas.has(aArea)) {
      return false;
    }
    return gBuildAreas.get(aArea).has(aInstance);
  },

  registerBuildArea: function(aArea, aNode) {
    if (!gBuildAreas.has(aArea)) {
      gBuildAreas.set(aArea, new Set());
    }

    gBuildAreas.get(aArea).add(aNode);
  },

  unregisterBuildWindow: function(aWindow) {
    let document = aWindow.document;

    for (let [, area] of gBuildAreas) {
      for (let node of area) {
        if (node.ownerDocument == document) {
          area.delete(node);
        }
      }
    }

    for (let [,widget] of gPalette)
      widget.instances.delete(document);
  },

  handleEvent: function(aEvent) {
    switch (aEvent.type) {
      case "unload": {
        let window = aEvent.target;
        window.removeEventListener("unload", this);
        this.unregisterBuildWindow(window);
        break;
      }
    }
  },

  isSpecialWidget: function(aId) {
    //XXXunf Need to double check these are getting handled correctly
    //       everywhere, notably save/restore.
    return (aId.startsWith("separator") ||
            aId.startsWith("spring") ||
            aId.startsWith("spacer"));
  },

  createSpecialWidget: function(aId, aDocument) {
    let node = aDocument.createElementNS(kNSXUL, "toolbar" + aId);
    // Due to timers resolution Date.now() can be the same for
    // elements created in small timeframes.  So ids are
    // differentiated through a unique count suffix.
    node.id = aId + Date.now() + (++gNewElementCount);
    if (aId == "spring") {
      node.flex = 1;
    }
    return node;
  },

  findWidgetInToolbox: function(aId, aToolbox, aDocument) {
    if (!aToolbox) {
      return null;
    }

    // look for a node with the same id, as the node may be
    // in a different toolbar.
    let node = aDocument.getElementById(aId);
    if (node) {
      let parent = node.parentNode;
      while (parent && parent.localName != "toolbar")
        parent = parent.parentNode;

      if (parent &&
          parent.toolbox == aToolbox &&
          parent.customizationTarget == node.parentNode) {
        return node;
      }
    }

    if (aToolbox.palette) {
      // Attempt to locate a node with a matching ID within
      // the palette.
      return aToolbox.palette.querySelector("#" + aId);
    }
    return null;
  },

  // Depending on aMenu here is a hack that should be fixed.
  buildWidget: function(aDocument, aMenu, aWidget) {
    if (typeof aWidget == "string") {
      aWidget = gPalette.get(aWidget);
    }
    if (!aWidget) {
      throw new Error("buildWidget was passed a non-widget to build.");
    }

    LOG("Building " + aWidget.id);

    let node = aDocument.createElementNS(kNSXUL, "toolbarbutton");

    node.setAttribute("id", aWidget.id);
    node.setAttribute("widget-id", aWidget.id);
    node.setAttribute("widget-type", aWidget.type);
    if (aWidget.disabled) {
      node.setAttribute("disabled", true);
    }
    node.setAttribute("label", aWidget.name);
    node.setAttribute("tooltiptext", aWidget.description);
    //XXXunf Need to hook this up to a <key> element or something.
    if (aWidget.shortcut) {
      node.setAttribute("acceltext", aWidget.shortcut);
    }
    node.setAttribute("class", "toolbarbutton-1 chromeclass-toolbar-additional");

    if (aMenu) {
      node.setAttribute("image", aWidget.icons["32"]);
    } else {
      node.setAttribute("image", aWidget.icons["16"]);
    }

    let handler = this.handleWidgetClick.bind(this, aWidget, node);
    node.addEventListener("command", handler, false);

    // If the widget has a view, and has view showing / hiding listeners,
    // hook those up to this widget.
    if (aWidget.type == "view" &&
        (aWidget.onViewShowing || aWidget.onViewHiding)) {
      LOG("Widget " + aWidget.id + " has a view with showing and hiding events. Auto-registering event handlers.");
      let viewNode = aDocument.getElementById(aWidget.viewId);

      if (!viewNode) {
        ERROR("Could not find the view node with id: " + aWidget.viewId);
        throw new Error("Could not find the view node with id: " + aWidget.viewId);
      }

      for (let eventName of kSubviewEvents) {
        let handler = "on" + eventName;
        if (typeof aWidget[handler] == "function") {
          viewNode.addEventListener(eventName, aWidget[handler], false);
        }
      }

      LOG("Widget " + aWidget.id + " showing and hiding event handlers set.");
    }

    aWidget.instances.set(aDocument, node);
    return node;
  },

  handleWidgetClick: function(aWidget, aNode, aEvent) {
    LOG("handleWidgetClick");

    if (aWidget.type == "button") {
      this.maybeAutoHidePanel(aEvent);

      if (aWidget.onCommand) {
        try {
          aWidget.onCommand.call(null, aEvent);
        } catch (e) {
          Cu.reportError(e);
        }
      } else {
        //XXXunf Need to think this through more, and formalize.
        Services.obs.notifyObservers(aNode,
                                     "customizedui-widget-click",
                                     aWidget.id);
      }
    } else if (aWidget.type == "view") {
      let ownerWindow = aNode.ownerDocument.defaultView;
      ownerWindow.PanelUI.showSubView(aWidget.viewId, aNode);
    }
  },

  maybeAutoHidePanel: function(aEvent) {
    if (aEvent.type == "keypress" && 
        !(aEvent.keyCode == aEvent.DOM_VK_ENTER ||
          aEvent.keyCode == aEvent.DOM_VK_RETURN)) {
      return;
    }

    if (aEvent.type == "mouseup" && aEvent.button != 0) {
      return;
    }

    let ownerWindow = aEvent.target.ownerDocument.defaultView;
    ownerWindow.PanelUI.hide();
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
      if (!this.getPlacementOfWidget(node.id)) {
        widgets.add(node.id);
      }
    }

    return [i for (i of widgets)];
  },

  getPlacementOfWidget: function(aWidgetId) {
    let widget = gPalette.get(aWidgetId);

    for (let [area, placements] of gPlacements) {
      let index = placements.indexOf(aWidgetId);
      if (index != -1) {
        return { area: area, position: index };
      }
    }

    return null;
  },

  addWidgetToArea: function(aWidgetId, aArea, aPosition) {
    if (!gAreas.has(aArea)) {
      throw new Error("Unknown customization area");
    }

    // If this is a lazy area that hasn't been restored yet, we can't yet modify
    // it - would would at least like to add to it. So we keep track of it in
    // gFuturePlacements,  and use that to add it when restoring the area. We
    // throw away aPosition though, as that can only be bogus if the area hasn't
    // yet been restorted (caller can't possibly know where its putting the
    // widget in relation to other widgets).
    if (this.isAreaLazy(aArea)) {
      gFuturePlacements.get(aArea).set(aWidgetId);
      return;
    }

    let oldPlacement = this.getPlacementOfWidget(aWidgetId);
    if (oldPlacement && oldPlacement.area == aArea) {
      this.moveWidgetWithinArea(aWidgetId, aPosition);
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

    gDirty = true;
    this.saveState();

    this.notifyListeners("onWidgetAdded", aWidgetId, aArea, aPosition);
  },

  removeWidgetFromArea: function(aWidgetId) {
    let oldPlacement = this.getPlacementOfWidget(aWidgetId);
    if (!oldPlacement) {
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

    this.notifyListeners("onWidgetRemoved", aWidgetId, oldPlacement.area);
  },

  moveWidgetWithinArea: function(aWidgetId, aPosition) {
    let oldPlacement = this.getPlacementOfWidget(aWidgetId);
    if (!oldPlacement) {
      return;
    }

    let placements = gPlacements.get(oldPlacement.area);
    if (aPosition < 0) {
      aPosition = 0;
    } else if (aPosition > placements.length - 1) {
      aPosition = placements.length - 1;
    }

    if (aPosition == oldPlacement.position) {
      return;
    }

    placements.splice(oldPlacement.position, 1);
    placements.splice(aPosition, 0, aWidgetId);

    let widget = gPalette.get(aWidgetId);
    if (widget) {
      widget.currentPosition = aPosition;
    }

    gDirty = true;
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
      if (gDefaultPlacements.has(aArea)) {
        let defaults = gDefaultPlacements.get(aArea);
        for (let id of defaults)
          this.addWidgetToArea(id, aArea);
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
    if (gInBatch || !gDirty) {
      return;
    }
    let state = { placements: gPlacements,
                  seen: gSeenWidgets };

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
      return [i for (i of aValue)];
    }

    return aValue;
  },

  beginBatchUpdate: function() {
    gInBatch = true;
  },

  endBatchUpdate: function(aForceSave) {
    gInBatch = false;
    if (aForceSave === true) {
      gDirty = true;
    }
    this.saveState();
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
        if (aEvent in listener) {
          listener[aEvent].apply(listener, aArgs);
        }
      } catch (e) {
        Cu.reportError(e + " -- " + e.fileName + ":" + e.lineNumber);
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
      let area = widget.defaultArea;
      if (gDefaultPlacements.has(area)) {
        gDefaultPlacements.get(area).push(widget.id);
      } else {
        gDefaultPlacements.set(area, [widget.id]);
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
            gFuturePlacements.get(widget.defaultArea).set(widget.id);
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
    // openned - so we know there's no build areas to handle. Also, builtin
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
      source: aSource || "addon",
      instances: new Map(),
      currentArea: null,
      defaultArea: null,
      allowedAreas: [],
      shortcut: null,
      description: null,
      icons: {}
    };

    if (typeof aData.id != "string" || !/^[a-z0-9-_]{1,}$/i.test(aData.id)) {
      ERROR("Given an illegal id in normalizeWidget: " + aData.id);
      return null;
    }

    const kReqStringProps = ["id", "name"];
    for (let prop of kReqStringProps) {
      if (typeof aData[prop] != "string") {
        return null;
      }
      widget[prop] = aData[prop];
    }

    const kOptStringProps = ["description", "shortcut"];
    for (let prop of kOptStringProps) {
      if (typeof aData[prop] == "string") {
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

    if (typeof aData.icons == "object") {
      let sizes = Object.keys(aData.icons);
      for (let size of sizes) {
        if (size == parseInt(size, 10)) {
          widget.icons[size] = aData.icons[size];
        }
      }
    }

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

      widget.onViewShowing = typeof aData.onViewShowing == "function" ?
                                 aData.onViewShowing :
                                 null;
      widget.onViewHiding = typeof aData.onViewHiding == "function" ? 
                                 aData.onViewHiding :
                                 null;
    }

    if (gPalette.has(widget.id)) {
      return null;
    }

    return widget;
  },

  destroyWidget: function(aWidgetId) {
    let widget = gPalette.get(aWidgetId);
    if (!widget) {
      return;
    }

    // This will not remove the widget from gPlacements - we want to keep the
    // setting so the widget gets put back in it's old position if/when it
    // returns.

    let area = widget.currentArea;
    if (area) {
      let buildArea = gBuildAreas.get(area);
      for (let buildNode of buildArea) {
        let widgetNode = buildNode.ownerDocument.getElementById(aWidgetId);
        if (widgetNode) {
          widgetNode.parentNode.removeChild(widgetNode);
        }
        for (let eventName of kSubviewEvents) {
          let handler = "on" + eventName;
          if (typeof widget[handler] == "function") {
            viewNode.removeEventListener(eventName, widget[handler], false);
          }
        }
      }
    }

    gPalette.delete(aWidgetId);

    this.notifyListeners("onWidgetDestroyed", aWidgetId);
  },

  registerManifest: function(aBaseLocation, aData) {
    let tokens = aData.split(/\s+/);
    let directive = tokens.shift();
    if (directive != "widget") {
      return;
    }

    for (let [id, widget] of gPalette) {
      if (widget.source == aBaseLocation.spec) {
        return; // Already registered.
      }
    }

    let uri = NetUtil.newURI(tokens.shift(), null, aBaseLocation);

    dump("\tNew widget! " + uri.spec + "\n");

    let data = "";
    try {
      if (uri.schemeIs("jar")) {
        data = this.readManifestFromJar(uri);
      } else {
        data = this.readManifestFromFile(uri);
      }
    }
    catch (e) {
      Cu.reportError(e);
      return;
    }
    data = JSON.parse(data);
    data.source = aBaseLocation.spec;

    this.createWidget(data);
  },

  // readManifestFromJar and readManifestFromFile from ChromeManifestParser.jsm.
  readManifestFromJar: function(aURI) {
    let data = "";
    let entries = [];
    let readers = [];

    try {
      // Deconstrict URI, which can be nested jar: URIs. 
      let uri = aURI.clone();
      while (uri instanceof Ci.nsIJARURI) {
        entries.push(uri.JAREntry);
        uri = uri.JARFile;
      }

      // Open the base jar.
      let reader = Cc["@mozilla.org/libjar/zip-reader;1"]
                     .createInstance(Ci.nsIZipReader);
      reader.open(uri.QueryInterface(Ci.nsIFileURL).file);
      readers.push(reader);

      // Open the nested jars.
      for (let i = entries.length - 1; i > 0; i--) {
        let innerReader = Cc["@mozilla.org/libjar/zip-reader;1"].
                          createInstance(Ci.nsIZipReader);
        innerReader.openInner(reader, entries[i]);
        readers.push(innerReader);
        reader = innerReader;
      }

      // First entry is the actual file we want to read.
      let zis = reader.getInputStream(entries[0]);
      data = NetUtil.readInputStreamToString(zis, zis.available());
    }
    finally {
      // Close readers in reverse order.
      for (let i = readers.length - 1; i >= 0; i--) {
        readers[i].close();
        //XXXunf Don't think this is needed, but need to double check.
        //flushJarCache(readers[i].file);
      }
    }

    return data;
  },

  readManifestFromFile: function(aURI) {
    let file = aURI.QueryInterface(Ci.nsIFileURL).file;
    if (!file.exists() || !file.isFile()) {
      return "";
    }

    let data = "";
    let fis = Cc["@mozilla.org/network/file-input-stream;1"]
                .createInstance(Ci.nsIFileInputStream);
    try {
      fis.init(file, -1, -1, false);
      data = NetUtil.readInputStreamToString(fis, fis.available());
    } finally {
      fis.close();
    }
    return data;
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
    Services.prefs.clearUserPref(kPrefCustomizationState);
    LOG("State reset");
  }
};
Object.freeze(CustomizableUIInternal);

this.CustomizableUI = {
  get AREA_PANEL() "PanelUI-contents",
  get AREA_NAVBAR() "nav-bar",

  get PROVIDER_XUL() "xul",
  get PROVIDER_API() "api",
  get PROVIDER_SPECIAL() "special",

  get SOURCE_BUILTIN() "builtin",
  get SOURCE_EXTERNAL() "external",

  get TYPE_BUTTON() "button",

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
  registerWindow: function(aWindow) {
    CustomizableUIInternal.registerWindow(aWindow);
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
  endBatchUpdate: function(aForceSave) {
    CustomizableUIInternal.endBatchUpdate(aForceSave);
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
  getWidgetsInArea: function(aArea) {
    if (!gAreas.has(aArea)) {
      throw new Error("Unknown customization area");
    }
    if (!gPlacements.has(aArea)) {
      throw new Error("Area not yet restored");
    }

    return gPlacements.get(aArea).map(
      CustomizableUIInternal.wrapWidget,
      CustomizableUIInternal
    );
  },
  get areas() {
    return [area for ([area, props] of gAreas)];
  },
  getCustomizeTargetForArea: function(aArea, aWindow) {
    return CustomizableUIInternal.getCustomizeTargetForArea(aArea, aWindow);
  },
  reset: function() {
    CustomizableUIInternal.reset();
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

  const kBareProps = ["id", "source", "type", "disabled", "name", "description"];
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
    let instance = aWidget.instances.get(aWindow.document);
    if (!instance) {
      instance = CustomizableUIInternal.buildWidget(aWindow.document,
                                                    null,
                                                    aWidget);
    }

    let wrapper = gWrapperCache.get(instance);
    if (!wrapper) {
      wrapper = new WidgetSingleWrapper(aWidget, instance);
      gWrapperCache.set(instance, wrapper);
    }
    return wrapper;
  };

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

  const nodeProps = ["label", "description"];
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

  let nodes = [];

  let placement = CustomizableUIInternal.getPlacementOfWidget(aWidgetId);
  if (placement) {
    let buildAreas = gBuildAreas.get(placement.area) || [];
    for (let areaNode of buildAreas)
      nodes.push(areaNode.ownerDocument.getElementById(aWidgetId));
  }

  this.id = aWidgetId;
  this.type = "custom";
  this.provider = CustomizableUI.PROVIDER_XUL;

  this.forWindow = function XULWidgetGroupWrapper_forWindow(aWindow) {
    let instance = aWindow.document.getElementById(aWidgetId);
    if (!instance) {
      // Toolbar palettes aren't part of the document, so elements in there
      // won't be found via document.getElementById().
      instance = aWindow.gNavToolbox.palette.querySelector("#" + aWidgetId);
    }

    let wrapper = gWrapperCache.get(instance);
    if (!wrapper) {
      wrapper = new XULWidgetSingleWrapper(aWidgetId, instance);
      gWrapperCache.set(instance, wrapper);
    }
    return wrapper;
  };

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

  Object.freeze(this);
}


CustomizableUIInternal.initialize();
