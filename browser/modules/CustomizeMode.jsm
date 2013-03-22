/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

this.EXPORTED_SYMBOLS = ["CustomizeMode"];

const {classes: Cc, interfaces: Ci, utils: Cu, results: Cr} = Components;

const kPrefCustomizationDebug = "browser.uiCustomization.debug";
const kPaletteId = "customization-palette";

let gDebug = false;
try {
  gDebug = Services.prefs.getBoolPref(kPrefCustomizationDebug);
} catch (e) {}

function LOG(str) {
  if (gDebug) {
    Services.console.logStringMessage(str);
  }
}

Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource:///modules/CustomizableUI.jsm");

function CustomizeMode(aWindow) {
  this.window = aWindow;
  // We register this window to have its customization data cleaned up when
  // unloading.
  CustomizableUI.registerWindow(this.window);

  this.document = aWindow.document;
  // There are two palettes - there's the palette that can be overlayed with
  // toolbar items in browser.xul. This is invisible, and never seen by the
  // user. Then there's the visible palette, which gets populated and displayed
  // to the user when in customizing mode.
  this.visiblePalette = this.document.getElementById(kPaletteId);
};

CustomizeMode.prototype = {
  _changed: false,
  window: null,
  document: null,
  // areas is used to cache the customizable areas when in customization mode.
  areas: null,
  // When in customizing mode, we swap out the reference to the invisible
  // palette in gNavToolbox.palette for our visiblePalette. This way, for the
  // customizing browser window, when widgets are removed from customizable
  // areas and added to the palette, they're added to the visible palette.
  // _stowedPalette is a reference to the old invisible palette so we can
  // restore gNavToolbox.palette to its original state after exiting
  // customization mode.
  _stowedPalette: null,
  _dragOverItem: null,

  enter: function() {
    let window = this.window;
    let document = this.document;

    CustomizableUI.addListener(this);

    // Add a keypress listener to the tab-view-deck so that we can quickly
    // exit customization mode when pressing ESC
    let tabViewDeck = document.getElementById("tab-view-deck");
    tabViewDeck.addEventListener("keypress", this, false);
    // Same goes for the menu button - if we're customizing, a click to the
    // menu button means a quick exit from customization mode.
    window.PanelUI.menuButton.addEventListener("click", this, false);
    window.PanelUI.menuButton.disabled = true;

    // Let everybody in this window know that we're about to customize.
    let evt = document.createEvent("CustomEvent");
    evt.initCustomEvent("CustomizationStart", true, true, window);
    window.dispatchEvent(evt);

    let customizer = document.getElementById("customization-container");
    customizer.parentNode.selectedPanel = customizer;

    window.PanelUI.hide();

    let self = this;
    let deck = document.getElementById("tab-view-deck");
    deck.addEventListener("transitionend", function customizeTransitionEnd() {
      deck.removeEventListener("transitionend", customizeTransitionEnd);

      // Move the mainView in the panel to the holder so that we can see it
      // while customizing.
      let panelHolder = document.getElementById("customization-panelHolder");
      panelHolder.appendChild(window.PanelUI.mainView);

      // Add drag-and-drop event handlers to all of the customizable areas.
      self.areas = [];
      for (let area of CustomizableUI.areas) {
        let target = CustomizableUI.getCustomizeTargetForArea(area, window);
        target.addEventListener("dragstart", self);
        target.addEventListener("dragover", self);
        target.addEventListener("dragexit", self);
        target.addEventListener("drop", self);
        for (let child of target.children) {
          self.wrapToolbarItem(child, getPlaceForItem(child));
        }
        self.areas.push(target);
      }

      self.populatePalette();
    });

    this.visiblePalette.addEventListener("dragstart", this);
    this.visiblePalette.addEventListener("dragover", this);
    this.visiblePalette.addEventListener("dragexit", this);
    this.visiblePalette.addEventListener("drop", this);

    document.documentElement.setAttribute("customizing", true);
  },

  exit: function(aToolboxChanged) {
    CustomizableUI.removeListener(this);

    let tabViewDeck = this.document.getElementById("tab-view-deck");
    tabViewDeck.removeEventListener("keypress", this, false);
    this.window.PanelUI.menuButton.removeEventListener("click", this, false);
    this.window.PanelUI.menuButton.disabled = false;

    this.depopulatePalette();

    this.visiblePalette.removeEventListener("dragstart", this);
    this.visiblePalette.removeEventListener("dragover", this);
    this.visiblePalette.removeEventListener("dragexit", this);
    this.visiblePalette.removeEventListener("drop", this);

    let window = this.window;
    let document = this.document;

    // Let everybody in this window know that we're finished customizing.
    let evt = document.createEvent("CustomEvent");
    evt.initCustomEvent("CustomizationEnd", true, true, {changed: this._changed});
    window.dispatchEvent(evt);

    if (this._changed) {
      // XXXmconley: At first, it seems strange to also persist the old way with
      //             currentset - but this might actually be useful for switching
      //             to old builds. We might want to keep this around for a little
      //             bit.
      this.persistCurrentSets();
    }

    document.documentElement.removeAttribute("customizing");

    for (let target of this.areas) {
      for (let toolbarItem of target.children) {
        this.unwrapToolbarItem(toolbarItem);
      }
      target.removeEventListener("dragstart", this);
      target.removeEventListener("dragover", this);
      target.removeEventListener("dragexit", this);
      target.removeEventListener("drop", this);
    }

    // And drop all area references.
    this.areas = [];

    window.PanelUI.replaceMainView(window.PanelUI.mainView);

    let browser = document.getElementById("browser");
    browser.parentNode.selectedPanel = browser;
    this._changed = false;
  },

  populatePalette: function() {
    let toolboxPalette = this.window.gNavToolbox.palette;

    let unusedWidgets = CustomizableUI.getUnusedWidgets(toolboxPalette);
    for (let widget of unusedWidgets) {
      let paletteItem = this.makePaletteItem(widget, "palette");
      this.visiblePalette.appendChild(paletteItem);
    }

    this._stowedPalette = this.window.gNavToolbox.palette;
    this.window.gNavToolbox.palette = this.visiblePalette;
  },

  //XXXunf Maybe this should use -moz-element instead of wrapping the node?
  //       Would ensure no weird interactions/event handling from original node,
  //       and makes it possible to put this in a lazy-loaded iframe/real tab
  //       while still getting rid of the need for overlays.
  makePaletteItem: function(aWidget, aPlace) {
    let widgetNode = aWidget.forWindow(this.window).node;
    let wrapper = this.createWrapper(widgetNode, aPlace);
    wrapper.appendChild(widgetNode);
    return wrapper;
  },

  depopulatePalette: function() {
    let wrapper = this.visiblePalette.firstChild;

    while (wrapper) {
      let widgetNode = wrapper.firstChild;

      let provider = CustomizableUI.getWidget(widgetNode.id);
      // If provider is PROVIDER_SPECIAL, then it just gets thrown away.
      if (provider = CustomizableUI.PROVIDER_XUL) {
        if (wrapper.hasAttribute("itemdisabled")) {
          widgetNode.disabled = true;
        }

        if (wrapper.hasAttribute("itemchecked")) {
          widgetNode.checked = true;
        }

        if (wrapper.hasAttribute("itemcommand")) {
          let commandID = wrapper.getAttribute("itemcommand");
          widgetNode.setAttribute("command", commandID);

          // Ensure node is in sync with its command after customizing.
          let command = this.document.getElementById(commandID);
          if (command && command.hasAttribute("disabled")) {
            widgetNode.setAttribute("disabled",
                                    command.getAttribute("disabled"));
          }
        }

        this._stowedPalette.appendChild(widgetNode);
      } else if (provider = CustomizableUI.PROVIDER_API) {
        //XXXunf Currently this doesn't destroy the (now unused) node. It would
        //       be good to do so, but we need to keep strong refs to it in
        //       CustomizableUI (can't iterate of WeakMaps), and there's the
        //       question of what behavior wrappers should have if consumers
        //       keep hold of them.
        //widget.destroyInstance(widgetNode);
      }

      this.visiblePalette.removeChild(wrapper);
      wrapper = this.visiblePalette.firstChild;
    }
    this.window.gNavToolbox.palette = this._stowedPalette;
  },

  wrapToolbarItem: function(aNode, aPlace) {
    let wrapper = this.createWrapper(aNode, aPlace);
    // It's possible that this toolbar node is "mid-flight" and doesn't have
    // a parent, in which case we skip replacing it. This can happen if a
    // toolbar item has been dragged into the palette. In that case, we tell
    // CustomizableUI to remove the widget from its area before putting the
    // widget in the palette - so the node will have no parent.
    if (aNode.parentNode) {
      aNode = aNode.parentNode.replaceChild(wrapper, aNode);
    }
    wrapper.appendChild(aNode);
    return wrapper;
  },

  createWrapper: function(aNode, aPlace) {
    let wrapper = this.document.createElement("toolbarpaletteitem");

    // "place" is used by toolkit to add the toolbarpaletteitem-palette
    // binding to a toolbarpaletteitem, which gives it a label node for when
    // it's sitting in the palette.
    wrapper.setAttribute("place", aPlace);

    // Ensure the wrapped item doesn't look like it's in any special state, and
    // can't be interactved with when in the customization palette.
    if (aNode.hasAttribute("command")) {
      wrapper.setAttribute("itemcommand", aNode.getAttribute("command"));
      aNode.removeAttribute("command");
    }

    if (aNode.checked) {
      wrapper.setAttribute("itemchecked", "true");
      aNode.checked = false;
    }

    if (aNode.disabled) {
      wrapper.setAttribute("itemdisabled", "true");
      aNode.disabled = false;
    }

    if (aNode.hasAttribute("id")) {
      wrapper.setAttribute("id", "wrapper-" + aNode.getAttribute("id"));
    }

    if (aNode.hasAttribute("title")) {
      wrapper.setAttribute("title", aNode.getAttribute("title"));
    } else if (aNode.hasAttribute("label")) {
      wrapper.setAttribute("title", aNode.getAttribute("label"));
    }

    if (aNode.hasAttribute("flex")) {
      wrapper.setAttribute("flex", aNode.getAttribute("flex"));
    }

    wrapper.addEventListener("mousedown", this);
    wrapper.addEventListener("mouseup", this);

    return wrapper;
  },

  unwrapToolbarItem: function(aWrapper) {
    aWrapper.removeEventListener("mousedown", this);
    aWrapper.removeEventListener("mouseup", this);

    let toolbarItem = aWrapper.firstChild;

    if (aWrapper.hasAttribute("itemdisabled")) {
      toolbarItem.disabled = true;
    }

    if (aWrapper.hasAttribute("itemchecked")) {
      toolbarItem.checked = true;
    }

    if (aWrapper.hasAttribute("itemcommand")) {
      let commandID = aWrapper.getAttribute("itemcommand");
      toolbarItem.setAttribute("command", commandID);

      //XXX Bug 309953 - toolbarbuttons aren't in sync with their commands after customizing
      let command = this.document.getElementById(commandID);
      if (command && command.hasAttribute("disabled")) {
        toolbarItem.setAttribute("disabled", command.getAttribute("disabled"));
      }
    }

    if (aWrapper.parentNode) {
      aWrapper.parentNode.replaceChild(toolbarItem, aWrapper);
    }
    return toolbarItem;
  },

  //XXXjaws This doesn't handle custom toolbars.
  //XXXmconley While CustomizableUI.jsm uses prefs to preserve state, we might
  //           also want to (try) persisting with currentset as well to make it
  //           less painful to switch to older builds.
  persistCurrentSets: function()  {
    //XXXjaws The toolbar bindings that are included in this changeset (/browser/base/content/toolbar.xml)
    //        don't implement currentSet. They probably need to inherit the toolkit bindings.
    return;

    let document = this.document;
    let toolbar = document.getElementById("nav-bar");

    // Calculate currentset and store it in the attribute.
    let currentSet = toolbar.currentSet;
    toolbar.setAttribute("currentset", currentSet);

    // Persist the currentset attribute directly on hardcoded toolbars.
    document.persist(toolbar.id, "currentset");
  },

  reset: function() {
    CustomizableUI.reset();
  },

  onWidgetMoved: function(aWidgetId, aArea, aOldPosition, aNewPosition) {
    this._changed = true;
  },

  onWidgetAdded: function(aWidgetId, aArea, aPosition) {
    this._changed = true;
  },

  onWidgetRemoved: function(aWidgetId, aArea) {
    this._changed = true;
  },

  onWidgetCreated: function(aWidgetId) {
  },

  onWidgetDestroyed: function(aWidgetId) {
  },

  handleEvent: function(aEvent) {
    switch(aEvent.type) {
      case "dragstart":
        this._onDragStart(aEvent);
        break;
      case "dragover":
        this._onDragOver(aEvent);
        break;
      case "drop":
        this._onDragDrop(aEvent);
        break;
      case "dragexit":
        this._onDragExit(aEvent);
        break;
      case "mousedown":
        this._onMouseDown(aEvent);
        break;
      case "mouseup":
        this._onMouseUp(aEvent);
        break;
      case "keypress":
        if (aEvent.keyCode === aEvent.DOM_VK_ESCAPE) {
          this.exit();
        }
        break;
      case "click":
        if (aEvent.button == 0 &&
            aEvent.originalTarget == this.window.PanelUI.menuButton) {
          this.exit();
          aEvent.preventDefault();
        }
        break;
    }
  },

  _onDragStart: function(aEvent) {
    __dumpDragData(aEvent);
    let item = aEvent.target;
    while (item && item.localName != "toolbarpaletteitem") {
      if (item.localName == "toolbar") {
        return;
      }
      item = item.parentNode;
    }

    let dt = aEvent.dataTransfer;
    let documentId = aEvent.target.ownerDocument.documentElement.id;
    dt.setData("text/toolbarwrapper-id/" + documentId, item.firstChild.id);
    dt.effectAllowed = "move";
  },

  _onDragOver: function(aEvent) {
    __dumpDragData(aEvent);

    let document = aEvent.target.ownerDocument;
    let documentId = document.documentElement.id;
    if (!aEvent.dataTransfer.types.contains("text/toolbarwrapper-id/"
                                            + documentId.toLowerCase())) {
      return;
    }

    let draggedItemId = aEvent.dataTransfer.getData("text/toolbarwrapper-id/" + documentId);
    let draggedWrapper = document.getElementById("wrapper-" + draggedItemId);
    let targetNode = aEvent.target;
    let targetParent = targetNode.parentNode;
    let targetArea = this._getCustomizableParent(targetNode);
    let originArea = this._getCustomizableParent(draggedWrapper);

    // Do nothing if the target or origin are not customizable.
    if (!targetArea || !originArea) {
      gCurrentDragOverItem = null;
      return;
    }

    // We need to determine the place that the widget is being dropped in
    // the target.
    let position = Array.indexOf(targetParent.children, targetNode);
    let dragOverItem = position == -1 ? targetParent.lastChild : targetParent.children[position];

    if (this._dragOverItem && dragOverItem != this._dragOverItem) {
      this._setDragActive(this._dragOverItem, false);
    }

    // XXXjaws Only handling the toolbar case first.
    if (targetArea.localName === "toolbar") {
      this._setDragActive(dragOverItem, true);
    }
    this._dragOverItem = dragOverItem;

    aEvent.preventDefault();
    aEvent.stopPropagation();
  },

  _onDragDrop: function(aEvent) {
    __dumpDragData(aEvent);

    this._setDragActive(this._dragOverItem, false);

    let document = aEvent.target.ownerDocument;
    let documentId = document.documentElement.id;
    let draggedItemId = aEvent.dataTransfer.getData("text/toolbarwrapper-id/" + documentId);
    let draggedWrapper = document.getElementById("wrapper-" + draggedItemId);

    draggedWrapper.removeAttribute("mousedown");

    let targetNode = aEvent.target;
    let targetParent = targetNode.parentNode;
    let targetArea = this._getCustomizableParent(targetNode);
    let originArea = this._getCustomizableParent(draggedWrapper);

    // Do nothing if the target area or origin area are not customizable.
    if (!targetArea || !originArea) {
      return;
    }

    // We need to determine the place that the widget is being dropped in
    // the target.
    let position = Array.indexOf(targetParent.children, targetNode);

    // Is the target area the customization palette? If so, we have two cases -
    // either the originArea was the palette, or a customizable area.
    if (targetArea.id === kPaletteId) {
      if (originArea.id !== kPaletteId) {
        this._removeParentFlex(draggedWrapper);
        let widget = this.unwrapToolbarItem(draggedWrapper);
        CustomizableUI.removeWidgetFromArea(draggedItemId);
        draggedWrapper = this.wrapToolbarItem(widget, "palette");
      }

      // If the targetNode is the palette itself, just append
      if (targetNode == this.visiblePalette) {
        this.visiblePalette.appendChild(draggedWrapper);
      } else {
        this.visiblePalette.insertBefore(draggedWrapper, targetNode);
      }
      return;
    }

    // Is the target area the same as the origin? Since we've already handled
    // the possibility that the target is the customization palette, we know
    // that the widget is moving within a customizable area.
    if (targetArea === originArea) {
      let widget = this.unwrapToolbarItem(draggedWrapper);
      CustomizableUI.moveWidgetWithinArea(draggedItemId, position);
      this.wrapToolbarItem(widget, getPlaceForItem(targetNode));
      return;
    }

    // We're moving from one customization area to another. Remove any flexing
    // that we might have added.
    this._removeParentFlex(draggedWrapper);

    // A little hackery - we quickly unwrap the item and use CustomizableUI's
    // addWidgetToArea to move the widget to the right place for every window,
    // then we re-wrap the widget.
    let widget = this.unwrapToolbarItem(draggedWrapper);
    CustomizableUI.addWidgetToArea(draggedItemId, targetArea.id, position);
    draggedWrapper = this.wrapToolbarItem(widget, getPlaceForItem(targetNode));

    // If necessary, add flex to accomodate new child.
    if (draggedWrapper.hasAttribute("flex")) {
      let parent = draggedWrapper.parentNode;
      let parentFlex = parent.hasAttribute("flex") ? parseInt(parent.getAttribute("flex"), 10) : 0;
      let itemFlex = parseInt(draggedWrapper.getAttribute("flex"), 10);
      parent.setAttribute("flex", parentFlex + itemFlex);
    }
  },

  _onDragExit: function(aEvent) {
    if (this._dragOverItem) {
      this._setDragActive(this._dragOverItem, false);
    }
  },

  // XXXjaws Show a ghost image or blank area where the item could be added, instead of black bar
  _setDragActive: function(aItem, aValue) {
    let node = aItem;
    let window = aItem.ownerDocument.defaultView;
    let direction = window.getComputedStyle(aItem, null).direction;
    let value = direction == "ltr"? "left" : "right";
    if (aItem.localName == "toolbar") {
      node = aItem.lastChild;
      value = direction == "ltr"? "right" : "left";
    }

    if (!node) {
      return;
    }

    if (aValue) {
      if (!node.hasAttribute("dragover")) {
        node.setAttribute("dragover", value);
      }
    } else {
      node.removeAttribute("dragover");
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

  _getCustomizableParent: function(aElement) {
    let areas = CustomizableUI.areas;
    areas.push(kPaletteId);
    while (aElement) {
      if (areas.indexOf(aElement.id) != -1) {
        return aElement;
      }
      aElement = aElement.parentNode;
    }
    return null;
  },

  _onMouseDown: function(aEvent) {
    let item = this._getWrapper(aEvent.target);
    if (item) {
      item.setAttribute("mousedown", "true");
    }
  },

  _onMouseUp: function(aEvent) {
    let item = this._getWrapper(aEvent.target);
    if (item) {
      item.removeAttribute("mousedown");
    }
  },

  _getWrapper: function(aElement) {
    while (aElement && aElement.localName != "toolbarpaletteitem") {
      if (aElement.localName == "toolbar")
        return null;
      aElement = aElement.parentNode;
    }
    return aElement;
  }
};

function getPlaceForItem(aElement) {
  let place;
  let node = aElement;
  while (node && !place) {
    if (node.localName == "toolbar")
      place = "toolbar";
    else if (node.id == CustomizableUI.AREA_PANEL)
      place = "panel";
    else if (node.id == kPaletteId)
      place = "palette";

    node = node.parentNode;
  }
  return place;
}

function __dumpDragData(aEvent, caller) {
  let str = "Dumping drag data (CustomizeMode.jsm) {\n";
  str += "  type: " + aEvent["type"] + "\n";
  for (let el of ["target", "currentTarget", "relatedTarget"]) {
    if (aEvent[el]) {
      str += "  " + el + ": " + aEvent[el] + "(localName=" + aEvent[el].localName + "; id=" + aEvent[el].id + ")\n";
    }
  }
  for (let prop in aEvent.dataTransfer) {
    if (typeof aEvent.dataTransfer[prop] != "function") {
      str += "  dataTransfer[" + prop + "]: " + aEvent.dataTransfer[prop] + "\n";
    }
  }
  str += "}";
  LOG(str);
}
