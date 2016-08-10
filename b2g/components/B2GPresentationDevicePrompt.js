/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

function debug(aMsg) {
  //dump("-*- B2GPresentationDevicePrompt: " + aMsg + "\n");
}

const { classes: Cc, interfaces: Ci, utils: Cu } = Components;

const kB2GPRESENTATIONDEVICEPROMPT_CONTRACTID = "@mozilla.org/presentation-device/prompt;1";
const kB2GPRESENTATIONDEVICEPROMPT_CID        = Components.ID("{4a300c26-e99b-4018-ab9b-c48cf9bc4de1}");

Cu.import("resource://gre/modules/XPCOMUtils.jsm");

XPCOMUtils.defineLazyModuleGetter(this, "SystemAppProxy",
                                  "resource://gre/modules/SystemAppProxy.jsm");

function B2GPresentationDevicePrompt() {}

B2GPresentationDevicePrompt.prototype = {
  classID: kB2GPRESENTATIONDEVICEPROMPT_CID,
  contractID: kB2GPRESENTATIONDEVICEPROMPT_CONTRACTID,
  classDescription: "B2G Presentation Device Prompt",
  QueryInterface: XPCOMUtils.generateQI([Ci.nsIPresentationDevicePrompt]),

  // nsIPresentationDevicePrompt
  promptDeviceSelection: function(aRequest) {
    let self = this;
    let requestId = Cc["@mozilla.org/uuid-generator;1"]
                      .getService(Ci.nsIUUIDGenerator).generateUUID().toString();

    SystemAppProxy.addEventListener("mozContentEvent", function contentEvent(aEvent) {
      let detail = aEvent.detail;
      if (detail.id !== requestId) {
        return;
      }

      SystemAppProxy.removeEventListener("mozContentEvent", contentEvent);

      switch (detail.type) {
        case "presentation-select-result":
          debug("device " + detail.deviceId + " is selected by user");
          let device = self._getDeviceById(detail.deviceId);
          if (!device) {
            debug("cancel request because device is not found");
            aRequest.cancel(Cr.NS_ERROR_DOM_NOT_FOUND_ERR);
          }
          aRequest.select(device);
          break;
        case "presentation-select-deny":
          debug("request canceled by user");
          aRequest.cancel(Cr.NS_ERROR_DOM_NOT_ALLOWED_ERR);
          break;
      }
    });

    let detail = {
      type: "presentation-select-device",
      origin: aRequest.origin,
      requestURL: aRequest.requestURL,
      id: requestId,
    };

    SystemAppProxy.dispatchEvent(detail);
  },

  _getDeviceById: function(aDeviceId) {
    let deviceManager = Cc["@mozilla.org/presentation-device/manager;1"]
                          .getService(Ci.nsIPresentationDeviceManager);
    let devices = deviceManager.getAvailableDevices().QueryInterface(Ci.nsIArray);

    for (let i = 0; i < devices.length; i++) {
      let device = devices.queryElementAt(i, Ci.nsIPresentationDevice);
      if (device.id === aDeviceId) {
        return device;
      }
    }

    return null;
  },
};

this.NSGetFactory = XPCOMUtils.generateNSGetFactory([B2GPresentationDevicePrompt]);
