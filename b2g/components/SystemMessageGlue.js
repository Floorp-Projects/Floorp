/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict"

const Cc = Components.classes;
const Ci = Components.interfaces;
const Cu = Components.utils;

Cu.import("resource://gre/modules/XPCOMUtils.jsm");
Cu.import("resource://gre/modules/Services.jsm");

XPCOMUtils.defineLazyModuleGetter(this, "SystemAppProxy",
                                  "resource://gre/modules/SystemAppProxy.jsm");

function SystemMessageGlue() {
}

SystemMessageGlue.prototype = {
  openApp: function(aPageURL, aManifestURL, aType, aTarget, aShowApp,
                    aOnlyShowApp, aExtra) {
    let payload = { url: aPageURL,
                    manifestURL: aManifestURL,
                    isActivity: (aType == "activity"),
                    target: aTarget,
                    showApp: aShowApp,
                    onlyShowApp: aOnlyShowApp,
                    expectingSystemMessage: true,
                    extra: aExtra };

    // |SystemAppProxy| will queue "open-app" events for non-activity system
    // messages without actually sending them until the system app is ready.
    SystemAppProxy._sendCustomEvent("open-app", payload, (aType == "activity"));
  },

  classID: Components.ID("{2846f034-e614-11e3-93cd-74d02b97e723}"),

  QueryInterface: XPCOMUtils.generateQI([Ci.nsISystemMessageGlue])
};

this.NSGetFactory = XPCOMUtils.generateNSGetFactory([SystemMessageGlue]);
