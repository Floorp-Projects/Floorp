/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict"

const {classes: Cc, interfaces: Ci, utils: Cu, results: Cr} = Components;

this.EXPORTED_SYMBOLS = ["DataStoreServiceInternal"];

function debug(s) {
  // dump('DEBUG DataStoreServiceInternal: ' + s + '\n');
}

Cu.import("resource://gre/modules/XPCOMUtils.jsm");
Cu.import("resource://gre/modules/Services.jsm");

XPCOMUtils.defineLazyServiceGetter(this, "ppmm",
                                   "@mozilla.org/parentprocessmessagemanager;1",
                                   "nsIMessageBroadcaster");

XPCOMUtils.defineLazyServiceGetter(this, "dataStoreService",
                                   "@mozilla.org/datastore-service;1",
                                   "nsIDataStoreService");

this.DataStoreServiceInternal = {
  init: function() {
    debug("init");

    let inParent = Cc["@mozilla.org/xre/app-info;1"].getService(Ci.nsIXULRuntime)
                      .processType == Ci.nsIXULRuntime.PROCESS_TYPE_DEFAULT;
    if (inParent) {
      ppmm.addMessageListener("DataStore:Get", this);
    }
  },

  receiveMessage: function(aMessage) {
    debug("receiveMessage");

    if (aMessage.name != 'DataStore:Get') {
      return;
    }

    let prefName = 'dom.testing.datastore_enabled_for_hosted_apps';
    if ((Services.prefs.getPrefType(prefName) == Services.prefs.PREF_INVALID ||
         !Services.prefs.getBoolPref(prefName)) &&
        !aMessage.target.assertAppHasStatus(Ci.nsIPrincipal.APP_STATUS_CERTIFIED)) {
      return;
    }

    let msg = aMessage.data;

    if (!aMessage.principal ||
        aMessage.principal.appId == Ci.nsIScriptSecurityManager.UNKNOWN_APP_ID) {
      aMessage.target.sendAsyncMessage("DataStore:Get:Return:KO");
      return;
    }

    msg.stores = dataStoreService.getDataStoresInfo(msg.name, aMessage.principal.appId);
    if (msg.stores === null) {
      aMessage.target.sendAsyncMessage("DataStore:Get:Return:KO");
      return;
    }
    aMessage.target.sendAsyncMessage("DataStore:Get:Return:OK", msg);
  }
}

DataStoreServiceInternal.init();
