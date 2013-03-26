# -*- Mode: javascript; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

/**
 * Customization handler prepares this browser window for entering and exiting
 * customization mode by handling CustomizationStart and CustomizationEnd
 * events.
 */
let CustomizationHandler = {
  handleEvent: function(aEvent) {
    switch(aEvent.type) {
      case "CustomizationStart":
        this._customizationStart();
        break;
      case "CustomizationEnd":
        this._customizationEnd(aEvent.detail);
        break;
    }
  },

  _customizationStart: function() {
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
    PlacesToolbarHelper.customizeStart();
    BookmarksMenuButton.customizeStart();
    DownloadsButton.customizeStart();
    TabsInTitlebar.allowedBy("customizing-toolbars", false);

    gBrowser.selectedTab = window.gBrowser.addTab("about:customizing");
  },

  _customizationEnd: function(aDetails) {
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
    BookmarksMenuButton.customizeDone();
    DownloadsButton.customizeDone();

    if (gBrowser.selectedBrowser.currentURI.spec == "about:customizing") {
      gBrowser.removeCurrentTab();
    }

    // The url bar splitter state is dependent on whether stop/reload
    // and the location bar are combined, so we need this ordering
    CombinedStopReload.init();
    UpdateUrlbarSearchSplitterState();
    setUrlAndSearchBarWidthForConditionalForwardButton();

    // Update the urlbar
    if (gURLBar) {
      URLBarSetURI();
      XULBrowserWindow.asyncUpdateUI();
      PlacesStarButton.updateState();
      SocialShareButton.updateShareState();
    }

    TabsInTitlebar.allowedBy("customizing-toolbars", true);

    // Re-enable parts of the UI we disabled during the dialog
    let menubar = document.getElementById("main-menubar");
    for (let childNode of menubar.childNodes)
      childNode.setAttribute("disabled", false);
    let cmd = document.getElementById("cmd_CustomizeToolbars");
    cmd.removeAttribute("disabled");

    // make sure to re-enable click-and-hold
    if (!getBoolPref("ui.click_hold_context_menus", false)) {
      SetClickAndHoldHandlers();
    }

    gBrowser.selectedBrowser.focus();
  }
}
