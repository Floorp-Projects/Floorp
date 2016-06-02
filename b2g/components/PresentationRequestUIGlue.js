/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict"

function debug(aMsg) {
  // dump("-*- PresentationRequestUIGlue: " + aMsg + "\n");
}

const { interfaces: Ci, utils: Cu, classes: Cc } = Components;

Cu.import("resource://gre/modules/XPCOMUtils.jsm");
Cu.import("resource://gre/modules/Services.jsm");

XPCOMUtils.defineLazyModuleGetter(this, "SystemAppProxy",
                                  "resource://gre/modules/SystemAppProxy.jsm");

function PresentationRequestUIGlue() { }

PresentationRequestUIGlue.prototype = {

  sendRequest: function(aUrl, aSessionId, aDevice) {
    let localDevice;
    try {
      localDevice = aDevice.QueryInterface(Ci.nsIPresentationLocalDevice);
    } catch (e) {}

    if (localDevice) {
      return this.sendTo1UA(aUrl, aSessionId, localDevice.windowId);
    } else {
      return this.sendTo2UA(aUrl, aSessionId);
    }
  },

  // For 1-UA scenario
  sendTo1UA: function(aUrl, aSessionId, aWindowId) {
    return new Promise((aResolve, aReject) => {
      let handler = (evt) => {
        if (evt.type === "unload") {
          SystemAppProxy.removeEventListenerWithId(aWindowId,
                                                   "unload",
                                                   handler);
          SystemAppProxy.removeEventListenerWithId(aWindowId,
                                                   "mozPresentationContentEvent",
                                                   handler);
          aReject();
        }
        if (evt.type === "mozPresentationContentEvent" &&
            evt.detail.id == aSessionId) {
          SystemAppProxy.removeEventListenerWithId(aWindowId,
                                                   "unload",
                                                   handler);
          SystemAppProxy.removeEventListenerWithId(aWindowId,
                                                   "mozPresentationContentEvent",
                                                   handler);
          this.appLaunchCallback(evt.detail, aResolve, aReject);
        }
      };
      // If system(-remote) app is closed.
      SystemAppProxy.addEventListenerWithId(aWindowId,
                                            "unload",
                                            handler);
      // Listen to the result for the opened iframe from front-end.
      SystemAppProxy.addEventListenerWithId(aWindowId,
                                            "mozPresentationContentEvent",
                                            handler);
      SystemAppProxy.sendCustomEventWithId(aWindowId,
                                           "mozPresentationChromeEvent",
                                           { type: "presentation-launch-receiver",
                                             url: aUrl,
                                             id: aSessionId });
    });
  },

  // For 2-UA scenario
  sendTo2UA: function(aUrl, aSessionId) {
    return new Promise((aResolve, aReject) => {
      let handler = (evt) => {
        if (evt.type === "mozPresentationContentEvent" &&
            evt.detail.id == aSessionId) {
          SystemAppProxy.removeEventListener("mozPresentationContentEvent",
                                            handler);
          this.appLaunchCallback(evt.detail, aResolve, aReject);
        }
      };

      // Listen to the result for the opened iframe from front-end.
      SystemAppProxy.addEventListener("mozPresentationContentEvent",
                                      handler);
      SystemAppProxy._sendCustomEvent("mozPresentationChromeEvent",
                                      { type: "presentation-launch-receiver",
                                        url: aUrl,
                                        id: aSessionId });
    });
  },

  appLaunchCallback: function(aDetail, aResolve, aReject) {
    switch(aDetail.type) {
      case "presentation-receiver-launched":
        aResolve(aDetail.frame);
        break;
      case "presentation-receiver-permission-denied":
        aReject();
        break;
    }
  },

  classID: Components.ID("{ccc8a839-0b64-422b-8a60-fb2af0e376d0}"),

  QueryInterface: XPCOMUtils.generateQI([Ci.nsIPresentationRequestUIGlue])
};

this.NSGetFactory = XPCOMUtils.generateNSGetFactory([PresentationRequestUIGlue]);
