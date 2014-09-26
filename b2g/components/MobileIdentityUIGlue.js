/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict"

const { interfaces: Ci, utils: Cu, classes: Cc } = Components;

Cu.import("resource://gre/modules/ContentRequestHelper.jsm");
Cu.import("resource://gre/modules/MobileIdentityCommon.jsm");
Cu.import("resource://gre/modules/MobileIdentityUIGlueCommon.jsm");
Cu.import("resource://gre/modules/Promise.jsm");

XPCOMUtils.defineLazyServiceGetter(this, "uuidgen",
                                   "@mozilla.org/uuid-generator;1",
                                   "nsIUUIDGenerator");

XPCOMUtils.defineLazyModuleGetter(this, "SystemAppProxy",
                                  "resource://gre/modules/SystemAppProxy.jsm");

const CHROME_EVENT = "mozMobileIdChromeEvent";
const CONTENT_EVENT = "mozMobileIdContentEvent";
const UNSOLICITED_CONTENT_EVENT = "mozMobileIdUnsolContentEvent";

function MobileIdentityUIGlue() {
  SystemAppProxy.addEventListener(UNSOLICITED_CONTENT_EVENT, this);
}

MobileIdentityUIGlue.prototype = {

  __proto__: ContentRequestHelper.prototype,

  _sendChromeEvent: function(aEventName, aData) {
    SystemAppProxy._sendCustomEvent(CHROME_EVENT, {
      eventName: aEventName,
      id: uuidgen.generateUUID().toString(),
      data: aData
    });
  },

  _oncancel: null,

  get oncancel() {
    return this._oncancel;
  },

  set oncancel(aCallback) {
    this._oncancel = aCallback;
  },

  _onresendcode: null,

  get onresendcode() {
    return this._onresendcode;
  },

  set onresendcode(aCallback) {
    this._onresendcode = aCallback;
  },

  startFlow: function(aManifestURL, aIccInfo) {
    let phoneNumberInfo;
    if (aIccInfo) {
      phoneNumberInfo = [];
      for (var i = 0; i < aIccInfo.length; i++) {
        let iccInfo = aIccInfo[i];
        phoneNumberInfo.push({
          primary: iccInfo.primary,
          msisdn: iccInfo.msisdn,
          operator: iccInfo.operator,
          external: iccInfo.external,
          serviceId: iccInfo.serviceId,
          mcc: iccInfo.mcc
        });
      }
    }

    return this.contentRequest(CONTENT_EVENT,
                               CHROME_EVENT,
                               "onpermissionrequest",
                               { phoneNumberInfo: phoneNumberInfo || [],
                                 manifestURL: aManifestURL })
    .then(
      (result) => {
        if (!result || !result.phoneNumber && !result.serviceId) {
          return Promise.reject(ERROR_INVALID_PROMPT_RESULT);
        }

        let promptResult = new MobileIdentityUIGluePromptResult(
          result.phoneNumber || null,
          result.prefix || null,
          result.mcc || null,
          result.serviceId || null
        );
        return promptResult;
      }
    );
  },

  verificationCodePrompt: function(aRetriesLeft, aTimeout, aTimeLeft) {
    return this.contentRequest(CONTENT_EVENT,
                               CHROME_EVENT,
                               "onverificationcode",
                               { retriesLeft: aRetriesLeft,
                                 verificationTimeout: aTimeout,
                                 verificationTimeoutLeft: aTimeLeft })
    .then(
      (result) => {
        if (!result || !result.verificationCode) {
          return Promise.reject(ERROR_INVALID_VERIFICATION_CODE);
        }

        return result.verificationCode;
      }
    );
  },

  error: function(aError) {
    log.error("UI error " + aError);
    this._sendChromeEvent("onerror", {
      error: aError
    });
  },

  verify: function() {
    this._sendChromeEvent("verify");
  },

  verified: function(aVerifiedPhoneNumber) {
    this._sendChromeEvent("onverified", {
      verifiedPhoneNumber: aVerifiedPhoneNumber
    });
  },

  handleEvent: function(aEvent) {
    let msg = aEvent.detail;
    if (!msg) {
      log.warning("Got invalid event");
      return;
    }
    log.debug("Got content event ${}", msg);

    switch(msg.eventName) {
      case 'cancel':
        this.oncancel();
        break;
      case 'resendcode':
        this.onresendcode();
        break;
      default:
        log.warning("Invalid event name");
        break;
    }
  },

  classID: Components.ID("{83dbe26a-81f3-4a75-9541-3d0b7ca496b5}"),

  QueryInterface: XPCOMUtils.generateQI([Ci.nsIMobileIdentityUIGlue])
};

this.NSGetFactory = XPCOMUtils.generateNSGetFactory([MobileIdentityUIGlue]);
