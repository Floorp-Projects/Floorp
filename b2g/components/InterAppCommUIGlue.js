/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict"

const Cc = Components.classes;
const Ci = Components.interfaces;
const Cu = Components.utils;

Cu.import("resource://gre/modules/XPCOMUtils.jsm");
Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource://gre/modules/Promise.jsm");

XPCOMUtils.defineLazyModuleGetter(this, "SystemAppProxy",
                                  "resource://gre/modules/SystemAppProxy.jsm");

const DEBUG = false;
function debug(aMsg) {
  dump("-- InterAppCommUIGlue: " + Date.now() + ": " + aMsg + "\n");
}

function InterAppCommUIGlue() {
  // This matrix is to store the callerID (a random UUID) / deferral binding.
  // An example of the object literal is shown below:
  //
  // {
  //   "callerID1" : deferred1,
  //   "callerID2" : deferred2
  // }
  this._deferreds = {};

  // Listen to the result of selected apps from front-end.
  SystemAppProxy.addEventListener ("mozIACContentEvent", function (aEvent) {
    let detail = aEvent.detail;
    if (detail.type != "inter-app-comm-permission") {
      return;
    }

    if (DEBUG) {
      debug("mozIACContentEvent: " + JSON.stringify(detail));
    }

    let callerID = detail.chromeEventID;
    let deferred = this._deferreds[callerID];
    if (!deferred) {
      if (DEBUG) {
        debug("Error! Cannot find the deferred for callerID: " + callerID);
      }
      return;
    }

    delete this._deferreds[callerID];
    deferred.resolve({ callerID: callerID,
                       keyword: detail.keyword,
                       manifestURL: detail.manifestURL,
                       selectedApps: detail.peers });
  }.bind(this));
}

InterAppCommUIGlue.prototype = {
  selectApps: function(aCallerID, aPubAppManifestURL, aKeyword, aAppsToSelect) {
    let deferred = Promise.defer();
    this._deferreds[aCallerID] = deferred;

    SystemAppProxy._sendCustomEvent("mozIACChromeEvent",
                                    { type: "inter-app-comm-permission",
                                      chromeEventID: aCallerID,
                                      manifestURL: aPubAppManifestURL,
                                      keyword: aKeyword,
                                      peers: aAppsToSelect });

    // TODO Bug 897169 Simulate the return of the app-selected result by
    // the prompt, which always allows the connection. This dummy codes
    // will be removed when the UX/UI for the prompt is ready.
    SystemAppProxy._sendCustomEvent("mozIACContentEvent",
                                    { type: "inter-app-comm-permission",
                                      chromeEventID: aCallerID,
                                      manifestURL: aPubAppManifestURL,
                                      keyword: aKeyword,
                                      peers: aAppsToSelect });

    return deferred.promise;
  },

  classID: Components.ID("{879ee66c-e246-11e3-9910-74d02b97e723}"),

  QueryInterface: XPCOMUtils.generateQI([Ci.nsIInterAppCommUIGlue])
};

this.NSGetFactory = XPCOMUtils.generateNSGetFactory([InterAppCommUIGlue]);
