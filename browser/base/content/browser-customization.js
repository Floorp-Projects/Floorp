# -*- Mode: javascript; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

/**
 * Customization handler prepares this browser window for entering and exiting
 * customization mode by handling customizationstarting and customizationending
 * events.
 */
let CustomizationHandler = {
  handleEvent: function(aEvent) {
    switch(aEvent.type) {
      case "customizationstarting":
        this._customizationStarting();
        break;
      case "customizationchange":
        this._customizationChange();
        break;
      case "customizationending":
        this._customizationEnding(aEvent.detail);
        break;
    }
  },

  isCustomizing: function() {
    return document.documentElement.hasAttribute("customizing");
  },

  _customizationStarting: function() {
    // Disable the toolbar context menu items
    let menubar = document.getElementById("main-menubar");
    for (let childNode of menubar.childNodes)
      childNode.setAttribute("disabled", true);

    let cmd = document.getElementById("cmd_CustomizeToolbars");
    cmd.setAttribute("disabled", "true");

    let splitter = document.getElementById("urlbar-search-splitter");
    if (splitter) {
      splitter.parentNode.removeChild(splitter);
    }

    CombinedStopReload.uninit();
    CombinedBackForward.uninit();
    PlacesToolbarHelper.customizeStart();
    BookmarkingUI.customizeStart();
    DownloadsButton.customizeStart();

    // The additional padding on the sides of the browser
    // can cause the customize tab to get clipped.
    let tabContainer = gBrowser.tabContainer;
    if (tabContainer.getAttribute("overflow") == "true") {
      let tabstrip = tabContainer.mTabstrip;
      tabstrip.ensureElementIsVisible(gBrowser.selectedTab, true);
    }
  },

  _customizationChange: function() {
    gHomeButton.updatePersonalToolbarStyle();
    BookmarkingUI.customizeChange();
    PlacesToolbarHelper.customizeChange();
  },

  _customizationEnding: function(aDetails) {
    // Update global UI elements that may have been added or removed
    if (aDetails.changed) {
      gURLBar = document.getElementById("urlbar");

      gProxyFavIcon = document.getElementById("page-proxy-favicon");
      gHomeButton.updateTooltip();
      gIdentityHandler._cacheElements();
      XULBrowserWindow.init();

#ifndef XP_MACOSX
      updateEditUIVisibility();
#endif

      // Hacky: update the PopupNotifications' object's reference to the iconBox,
      // if it already exists, since it may have changed if the URL bar was
      // added/removed.
      if (!window.__lookupGetter__("PopupNotifications")) {
        PopupNotifications.iconBox =
          document.getElementById("notification-popup-box");
      }

    }

    PlacesToolbarHelper.customizeDone();
    BookmarkingUI.customizeDone();
    DownloadsButton.customizeDone();

    // The url bar splitter state is dependent on whether stop/reload
    // and the location bar are combined, so we need this ordering
    CombinedStopReload.init();
    CombinedBackForward.init();
    UpdateUrlbarSearchSplitterState();

    // Update the urlbar
    if (gURLBar) {
      URLBarSetURI();
      XULBrowserWindow.asyncUpdateUI();
      BookmarkingUI.updateStarState();
    }

    // Re-enable parts of the UI we disabled during the dialog
    let menubar = document.getElementById("main-menubar");
    for (let childNode of menubar.childNodes)
      childNode.setAttribute("disabled", false);
    let cmd = document.getElementById("cmd_CustomizeToolbars");
    cmd.removeAttribute("disabled");

    gBrowser.selectedBrowser.focus();
  }
}
