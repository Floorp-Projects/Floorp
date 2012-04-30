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
      // New download notifications are already handled by the panel service.
      // We don't handle them here because we don't want them to depend on the
      // "browser.download.manager.showWhenStarting" and
      // "browser.download.manager.focusWhenStarting" preferences.
      return;
    }

    // Show the panel in the most recent browser window, if present.
    let browserWin = gBrowserGlue.getMostRecentBrowserWindow();
    if (browserWin) {
      // The most recent browser window could have been minimized, in that case
      // it must be restored to allow the panel to open properly.
      if (browserWin.windowState == Ci.nsIDOMChromeWindow.STATE_MINIMIZED) {
        browserWin.restore();
      }
      browserWin.focus();
      browserWin.DownloadsPanel.showPanel();
      return;
    }

    // If no browser window is visible and the user requested to show the
    // current downloads, try and open a new window.  We'll open the panel when
    // delayed loading is finished.
    Services.obs.addObserver(function DUIO_observe(aSubject, aTopic, aData) {
      Services.obs.removeObserver(DUIO_observe, aTopic);
      aSubject.DownloadsPanel.showPanel();
    }, "browser-delayed-startup-finished", false);

    // We must really build an empty arguments list for the new window.
    let windowFirstArg = Cc["@mozilla.org/supports-string;1"]
                         .createInstance(Ci.nsISupportsString);
    let windowArgs = Cc["@mozilla.org/supports-array;1"]
                     .createInstance(Ci.nsISupportsArray);
    windowArgs.AppendElement(windowFirstArg);
    Services.ww.openWindow(null, "chrome://browser/content/browser.xul",
                           null, "chrome,dialog=no,all", windowArgs);
  },

  get visible()
  {
    if (DownloadsCommon.useToolkitUI) {
      return this._toolkitUI.visible;
    }

    let browserWin = gBrowserGlue.getMostRecentBrowserWindow();
    return browserWin ? browserWin.DownloadsPanel.isPanelShowing : false;
  },

  getAttention: function DUI_getAttention()
  {
    if (DownloadsCommon.useToolkitUI) {
      this._toolkitUI.getAttention();
    }
  }
};

////////////////////////////////////////////////////////////////////////////////
//// Module

const NSGetFactory = XPCOMUtils.generateNSGetFactory([DownloadsUI]);
