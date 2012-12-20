/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * This component implements the nsIDownloadManagerUI interface and opens the
 * downloads panel in the most recent browser window when requested.
 *
 * If a specific preference is set, this component transparently forwards all
 * calls to the original implementation in Toolkit, that shows the window UI.
 */

"use strict";

////////////////////////////////////////////////////////////////////////////////
//// Globals

const Cc = Components.classes;
const Ci = Components.interfaces;
const Cu = Components.utils;
const Cr = Components.results;

Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource://gre/modules/XPCOMUtils.jsm");

XPCOMUtils.defineLazyModuleGetter(this, "DownloadsCommon",
                                  "resource:///modules/DownloadsCommon.jsm");
XPCOMUtils.defineLazyServiceGetter(this, "gBrowserGlue",
                                   "@mozilla.org/browser/browserglue;1",
                                   "nsIBrowserGlue");
XPCOMUtils.defineLazyModuleGetter(this, "RecentWindow",
                                  "resource:///modules/RecentWindow.jsm");

////////////////////////////////////////////////////////////////////////////////
//// DownloadsUI

function DownloadsUI()
{
  XPCOMUtils.defineLazyGetter(this, "_toolkitUI", function () {
    // Create Toolkit's nsIDownloadManagerUI implementation.
    return Components.classesByID["{7dfdf0d1-aff6-4a34-bad1-d0fe74601642}"]
                     .getService(Ci.nsIDownloadManagerUI);
  });
}

DownloadsUI.prototype = {
  classID: Components.ID("{4d99321e-d156-455b-81f7-e7aa2308134f}"),

  _xpcom_factory: XPCOMUtils.generateSingletonFactory(DownloadsUI),

  //////////////////////////////////////////////////////////////////////////////
  //// nsISupports

  QueryInterface: XPCOMUtils.generateQI([Ci.nsIDownloadManagerUI]),

  //////////////////////////////////////////////////////////////////////////////
  //// nsIDownloadManagerUI

  show: function DUI_show(aWindowContext, aID, aReason)
  {
    if (DownloadsCommon.useToolkitUI) {
      this._toolkitUI.show(aWindowContext, aID, aReason);
      return;
    }

    if (!aReason) {
      aReason = Ci.nsIDownloadManagerUI.REASON_USER_INTERACTED;
    }

    if (aReason == Ci.nsIDownloadManagerUI.REASON_NEW_DOWNLOAD) {
      const kMinimized = Ci.nsIDOMChromeWindow.STATE_MINIMIZED;
      let browserWin = gBrowserGlue.getMostRecentBrowserWindow();

      if (!browserWin || browserWin.windowState == kMinimized) {
        this._showDownloadManagerUI(aWindowContext, aID, aReason);
      }
      else {
        // If the indicator is visible, then new download notifications are
        // already handled by the panel service.
        browserWin.DownloadsButton.checkIsVisible(function(isVisible) {
          if (!isVisible) {
            this._showDownloadManagerUI(aWindowContext, aID, aReason);
          }
        }.bind(this));
      }
    } else {
      this._showDownloadManagerUI(aWindowContext, aID, aReason);
    }
  },

  get visible()
  {
    // If we're still using the toolkit downloads manager, delegate the call
    // to it. Otherwise, return true for now, until we decide on how we want
    // to indicate that a new download has started if a browser window is
    // not available or minimized.
    return DownloadsCommon.useToolkitUI ? this._toolkitUI.visible : true;
  },

  getAttention: function DUI_getAttention()
  {
    if (DownloadsCommon.useToolkitUI) {
      this._toolkitUI.getAttention();
    }
  },

  /**
   * Helper function that opens the right download manager UI. Either the
   * new Downloads View in Places, or the toolkit download window if the
   * Places Downloads View is not enabled.
   */
  _showDownloadManagerUI:
  function DUI_showDownloadManagerUI(aWindowContext, aID, aReason)
  {
    // First, determine if the Places Downloads view is preffed on.
    let usePlacesView = false;
    try {
      usePlacesView =
        Services.prefs.getBoolPref("browser.library.useNewDownloadsView");
    } catch(e) {}

    if (!usePlacesView) {
      // If we got here, then the browser.library.useNewDownloadsView pref
      // either didn't exist or was false, so just show the toolkit downloads
      // manager.
      this._toolkitUI.show(aWindowContext, aID, aReason);
      return;
    }

    let organizer = Services.wm.getMostRecentWindow("Places:Organizer");
    if (!organizer) {
      let parentWindow = aWindowContext;
      // If we weren't given a window context, try to find a browser window
      // to use as our parent - and if that doesn't work, error out and give
      // up.
      if (!parentWindow) {
        parentWindow = RecentWindow.getMostRecentBrowserWindow();
        if (!parentWindow) {
          Components.utils
                    .reportError("Couldn't find a browser window to open " +
                                 "the Places Downloads View from.");
          return;
        }
      }
      parentWindow.openDialog("chrome://browser/content/places/places.xul",
                              "", "chrome,toolbar=yes,dialog=no,resizable",
                              "Downloads");
    }
    else {
      organizer.PlacesOrganizer.selectLeftPaneQuery("Downloads");
      organizer.focus();
    }
  }
};

////////////////////////////////////////////////////////////////////////////////
//// Module

this.NSGetFactory = XPCOMUtils.generateNSGetFactory([DownloadsUI]);
