/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80 filetype=javascript: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * This component enables the JavaScript API for downloads at startup.  This
 * will eventually be removed when nsIDownloadManager will not be available
 * anymore (bug 851471).
 */

"use strict";

////////////////////////////////////////////////////////////////////////////////
//// Globals

const Cc = Components.classes;
const Ci = Components.interfaces;
const Cu = Components.utils;
const Cr = Components.results;

Cu.import("resource://gre/modules/XPCOMUtils.jsm");

/**
 * CID and Contract ID of our implementation of nsIDownloadManagerUI.
 */
const kDownloadsUICid = Components.ID("{4d99321e-d156-455b-81f7-e7aa2308134f}");
const kDownloadsUIContractId = "@mozilla.org/download-manager-ui;1";

/**
 * CID and Contract ID of the JavaScript implementation of nsITransfer.
 */
const kTransferCid = Components.ID("{1b4c85df-cbdd-4bb6-b04e-613caece083c}");
const kTransferContractId = "@mozilla.org/transfer;1";

////////////////////////////////////////////////////////////////////////////////
//// DownloadsStartup

function DownloadsStartup() { }

DownloadsStartup.prototype = {
  classID: Components.ID("{49507fe5-2cee-4824-b6a3-e999150ce9b8}"),

  _xpcom_factory: XPCOMUtils.generateSingletonFactory(DownloadsStartup),

  //////////////////////////////////////////////////////////////////////////////
  //// nsISupports

  QueryInterface: XPCOMUtils.generateQI([Ci.nsIObserver]),

  //////////////////////////////////////////////////////////////////////////////
  //// nsIObserver

  observe: function DS_observe(aSubject, aTopic, aData)
  {
    if (aTopic != "profile-after-change") {
      Cu.reportError("Unexpected observer notification.");
      return;
    }

    // Override Toolkit's nsIDownloadManagerUI implementation with our own.
    // This must be done at application startup and not in the manifest to
    // ensure that our implementation overrides the original one.
    Components.manager.QueryInterface(Ci.nsIComponentRegistrar)
                      .registerFactory(kDownloadsUICid, "",
                                       kDownloadsUIContractId, null);

    // Override Toolkit's nsITransfer implementation with the one from the
    // JavaScript API for downloads.
    Components.manager.QueryInterface(Ci.nsIComponentRegistrar)
                      .registerFactory(kTransferCid, "",
                                       kTransferContractId, null);
  },
};

////////////////////////////////////////////////////////////////////////////////
//// Module

this.NSGetFactory = XPCOMUtils.generateNSGetFactory([DownloadsStartup]);
