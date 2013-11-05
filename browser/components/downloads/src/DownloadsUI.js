/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80 filetype=javascript: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * This component implements the nsIDownloadManagerUI interface and opens the
 * Downloads view for the most recent browser window when requested.
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
XPCOMUtils.defineLazyModuleGetter(this, "PrivateBrowsingUtils",
                                  "resource://gre/modules/PrivateBrowsingUtils.jsm");

////////////////////////////////////////////////////////////////////////////////
//// DownloadsUI

function DownloadsUI()
{
}

DownloadsUI.prototype = {
  classID: Components.ID("{4d99321e-d156-455b-81f7-e7aa2308134f}"),

  _xpcom_factory: XPCOMUtils.generateSingletonFactory(DownloadsUI),

  //////////////////////////////////////////////////////////////////////////////
  //// nsISupports

  QueryInterface: XPCOMUtils.generateQI([Ci.nsIDownloadManagerUI]),

  //////////////////////////////////////////////////////////////////////////////
  //// nsIDownloadManagerUI

  show: function DUI_show(aWindowContext, aDownload, aReason, aUsePrivateUI)
  {
    if (!aReason) {
      aReason = Ci.nsIDownloadManagerUI.REASON_USER_INTERACTED;
    }

    if (aReason == Ci.nsIDownloadManagerUI.REASON_NEW_DOWNLOAD) {
      const kMinimized = Ci.nsIDOMChromeWindow.STATE_MINIMIZED;
      let browserWin = gBrowserGlue.getMostRecentBrowserWindow();

      if (!browserWin || browserWin.windowState == kMinimized) {
        this._showDownloadManagerUI(aWindowContext, aUsePrivateUI);
      }
      else {
        // If the indicator is visible, then new download notifications are
        // already handled by the panel service.
        browserWin.DownloadsButton.checkIsVisible(function(isVisible) {
          if (!isVisible) {
            this._showDownloadManagerUI(aWindowContext, aUsePrivateUI);
          }
        }.bind(this));
      }
    } else {
      this._showDownloadManagerUI(aWindowContext, aUsePrivateUI);
    }
  },

  get visible() true,

  getAttention: function () {},

  //////////////////////////////////////////////////////////////////////////////
  //// Private

  /**
   * Helper function that opens the download manager UI.
   */
  _showDownloadManagerUI: function (aWindowContext, aUsePrivateUI)
  {
    // If we weren't given a window context, try to find a browser window
    // to use as our parent - and if that doesn't work, error out and give up.
    let parentWindow = aWindowContext;
    if (!parentWindow) {
      parentWindow = RecentWindow.getMostRecentBrowserWindow({ private: !!aUsePrivateUI });
      if (!parentWindow) {
        Components.utils.reportError(
          "Couldn't find a browser window to open the Places Downloads View " +
          "from.");
        return;
      }
    }

    // If window is private then show it in a tab.
    if (PrivateBrowsingUtils.isWindowPrivate(parentWindow)) {
      parentWindow.openUILinkIn("about:downloads", "tab");
      return;
    } else {
      let organizer = Services.wm.getMostRecentWindow("Places:Organizer");
      if (!organizer) {
        parentWindow.openDialog("chrome://browser/content/places/places.xul",
                                "", "chrome,toolbar=yes,dialog=no,resizable",
                                "Downloads");
      } else {
        organizer.PlacesOrganizer.selectLeftPaneQuery("Downloads");
        organizer.focus();
      }
    }
  }
};

////////////////////////////////////////////////////////////////////////////////
//// Module

this.NSGetFactory = XPCOMUtils.generateNSGetFactory([DownloadsUI]);
