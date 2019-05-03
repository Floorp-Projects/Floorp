/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// This file is loaded into the browser window scope.
/* eslint-env mozilla/browser-window */

/**
 * Customization handler prepares this browser window for entering and exiting
 * customization mode by handling customizationstarting and customizationending
 * events.
 */
var CustomizationHandler = {
  handleEvent(aEvent) {
    switch (aEvent.type) {
      case "customizationstarting":
        this._customizationStarting();
        break;
      case "customizationending":
        this._customizationEnding(aEvent.detail);
        break;
    }
  },

  isCustomizing() {
    return document.documentElement.hasAttribute("customizing");
  },

  _customizationStarting() {
    // Disable the toolbar context menu items
    let menubar = document.getElementById("main-menubar");
    for (let childNode of menubar.children)
      childNode.setAttribute("disabled", true);

    let cmd = document.getElementById("cmd_CustomizeToolbars");
    cmd.setAttribute("disabled", "true");

    UpdateUrlbarSearchSplitterState();

    PlacesToolbarHelper.customizeStart();

    gURLBarHandler.customizeStart();
  },

  _customizationEnding(aDetails) {
    // Update global UI elements that may have been added or removed
    if (aDetails.changed &&
        AppConstants.platform != "macosx") {
      updateEditUIVisibility();
    }

    PlacesToolbarHelper.customizeDone();

    UpdateUrlbarSearchSplitterState();

    // Update the urlbar
    URLBarSetURI();
    XULBrowserWindow.asyncUpdateUI();

    // Re-enable parts of the UI we disabled during the dialog
    let menubar = document.getElementById("main-menubar");
    for (let childNode of menubar.children)
      childNode.setAttribute("disabled", false);
    let cmd = document.getElementById("cmd_CustomizeToolbars");
    cmd.removeAttribute("disabled");

    gBrowser.selectedBrowser.focus();

    gURLBarHandler.customizeEnd();
  },
};

var AutoHideMenubar = {
  get _node() {
    delete this._node;
    return this._node = document.getElementById("toolbar-menubar");
  },

  _contextMenuListener: {
    contextMenu: null,

    get active() {
      return !!this.contextMenu;
    },

    init(event) {
      // Ignore mousedowns in <menupopup>s.
      if (event.target.closest("menupopup")) {
        return;
      }

      let contextMenuId = AutoHideMenubar._node.getAttribute("context");
      this.contextMenu = document.getElementById(contextMenuId);
      this.contextMenu.addEventListener("popupshown", this);
      this.contextMenu.addEventListener("popuphiding", this);
      AutoHideMenubar._node.addEventListener("mousemove", this);
    },
    handleEvent(event) {
      switch (event.type) {
        case "popupshown":
          AutoHideMenubar._node.removeEventListener("mousemove", this);
          break;
        case "popuphiding":
        case "mousemove":
          AutoHideMenubar._setInactiveAsync();
          AutoHideMenubar._node.removeEventListener("mousemove", this);
          this.contextMenu.removeEventListener("popuphiding", this);
          this.contextMenu.removeEventListener("popupshown", this);
          this.contextMenu = null;
          break;
      }
    },
  },

  init() {
    this._node.addEventListener("toolbarvisibilitychange", this);
    if (this._node.getAttribute("autohide") == "true") {
      this._enable();
    }
  },

  _updateState() {
    if (this._node.getAttribute("autohide") == "true") {
      this._enable();
    } else {
      this._disable();
    }
  },

  _events: ["DOMMenuBarInactive", "DOMMenuBarActive", "popupshowing", "mousedown"],
  _enable() {
    this._node.setAttribute("inactive", "true");
    for (let event of this._events) {
      this._node.addEventListener(event, this);
    }
  },

  _disable() {
    this._setActive();
    for (let event of this._events) {
      this._node.removeEventListener(event, this);
    }
  },

  handleEvent(event) {
    switch (event.type) {
      case "toolbarvisibilitychange":
        this._updateState();
        break;
      case "popupshowing":
        // fall through
      case "DOMMenuBarActive":
        this._setActive();
        break;
      case "mousedown":
        if (event.button == 2) {
          this._contextMenuListener.init(event);
        }
        break;
      case "DOMMenuBarInactive":
        if (!this._contextMenuListener.active)
          this._setInactiveAsync();
        break;
    }
  },

  _setInactiveAsync() {
    this._inactiveTimeout = setTimeout(() => {
      if (this._node.getAttribute("autohide") == "true") {
        this._inactiveTimeout = null;
        this._node.setAttribute("inactive", "true");
      }
    }, 0);
  },

  _setActive() {
    if (this._inactiveTimeout) {
      clearTimeout(this._inactiveTimeout);
      this._inactiveTimeout = null;
    }
    this._node.removeAttribute("inactive");
  },
};
