/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict"

function debug(aMsg) {
  //dump("-*- PresentationRequestUIGlue: " + aMsg + "\n");
}

const { interfaces: Ci, utils: Cu, classes: Cc } = Components;

Cu.import("resource://gre/modules/XPCOMUtils.jsm");
Cu.import("resource://gre/modules/Services.jsm");

XPCOMUtils.defineLazyModuleGetter(this, "SystemAppProxy",
                                  "resource://gre/modules/SystemAppProxy.jsm");

function PresentationRequestUIGlue() {
  // This is to store the session ID / resolver binding.
  // An example of the object literal is shown below:
  //
  // {
  //   "sessionId1" : resolver1,
  //   ...
  // }
  this._resolvers = {};

  // Listen to the result for the opened iframe from front-end.
  SystemAppProxy.addEventListener("mozPresentationContentEvent", aEvent => {
    let detail = aEvent.detail;

    if (detail.type != "presentation-receiver-launched") {
      return;
    }

    let sessionId = detail.id;
    let resolver = this._resolvers[sessionId];
    if (!resolver) {
      debug("No correspondent resolver for session ID: " + sessionId);
      return;
    }

    delete this._resolvers[sessionId];
    resolver(detail.frame);
  });
}

PresentationRequestUIGlue.prototype = {

  sendRequest: function(aUrl, aSessionId) {
    return new Promise(function(aResolve, aReject) {
      this._resolvers[aSessionId] = aResolve;

      SystemAppProxy._sendCustomEvent("mozPresentationChromeEvent",
                                      { type: "presentation-launch-receiver",
                                        url: aUrl,
                                        id: aSessionId });
    }.bind(this));
  },

  classID: Components.ID("{ccc8a839-0b64-422b-8a60-fb2af0e376d0}"),

  QueryInterface: XPCOMUtils.generateQI([Ci.nsIPresentationRequestUIGlue])
};

this.NSGetFactory = XPCOMUtils.generateNSGetFactory([PresentationRequestUIGlue]);
