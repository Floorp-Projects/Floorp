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
    for (let childNode of menubar.childNodes)
      childNode.setAttribute("disabled", true);

    let cmd = document.getElementById("cmd_CustomizeToolbars");
    cmd.setAttribute("disabled", "true");

    UpdateUrlbarSearchSplitterState();

    PlacesToolbarHelper.customizeStart();
  },

  _customizationEnding(aDetails) {
    // Update global UI elements that may have been added or removed
    if (aDetails.changed) {
      gURLBar = document.getElementById("urlbar");

      gHomeButton.updateTooltip();

      if (AppConstants.platform != "macosx")
        updateEditUIVisibility();

      // Hacky: update the PopupNotifications' object's reference to the iconBox,
      // if it already exists, since it may have changed if the URL bar was
      // added/removed.
      if (!window.__lookupGetter__("PopupNotifications")) {
        PopupNotifications.iconBox =
          document.getElementById("notification-popup-box");
      }

    }

    PlacesToolbarHelper.customizeDone();

    UpdateUrlbarSearchSplitterState();

    // Update the urlbar
    URLBarSetURI();
    XULBrowserWindow.asyncUpdateUI();

    // Re-enable parts of the UI we disabled during the dialog
    let menubar = document.getElementById("main-menubar");
    for (let childNode of menubar.childNodes)
      childNode.setAttribute("disabled", false);
    let cmd = document.getElementById("cmd_CustomizeToolbars");
    cmd.removeAttribute("disabled");

    gBrowser.selectedBrowser.focus();
  }
};
