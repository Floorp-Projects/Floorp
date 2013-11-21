/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const DEBUG = false;
function debug(s) { dump("-*- PhoneNumberService.js: " + s + "\n"); }

const Cc = Components.classes;
const Ci = Components.interfaces;
const Cu = Components.utils;

Cu.import("resource://gre/modules/XPCOMUtils.jsm");
Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource://gre/modules/DOMRequestHelper.jsm");
Cu.import("resource://gre/modules/PhoneNumberUtils.jsm");
Cu.import("resource://gre/modules/PhoneNumberNormalizer.jsm");

XPCOMUtils.defineLazyServiceGetter(this, "cpmm",
                                   "@mozilla.org/childprocessmessagemanager;1",
                                   "nsIMessageSender");

// PhoneNumberService

function PhoneNumberService()
{
  if (DEBUG) debug("Constructor");
}

PhoneNumberService.prototype = {
  __proto__: DOMRequestIpcHelper.prototype,

  receiveMessage: function(aMessage) {
    if (DEBUG) debug("receiveMessage: " + aMessage.name);
    let msg = aMessage.json;

    let req = this.getRequest(msg.requestID);
    if (!req) {
      return;
    }

    switch (aMessage.name) {
      case "PhoneNumberService:FuzzyMatch:Return:KO":
        Services.DOMRequest.fireError(req.request, msg.errorMsg);
        break;
      case "PhoneNumberService:FuzzyMatch:Return:OK":
        Services.DOMRequest.fireSuccess(req.request, msg.result);
        break;
      default:
        if (DEBUG) debug("Wrong message: " + aMessage.name);
    }
    this.removeRequest(msg.requestID);
  },

  fuzzyMatch: function(aNumber1, aNumber2) {
    if (DEBUG) debug("fuzzyMatch: " + aNumber1 + ", " + aNumber2);
    let request = this.createRequest();

    if ((aNumber1 && !aNumber2) || (aNumber2 && !aNumber1)) {
      // if only one of the numbers is empty/null/undefined and the other
      // number is not, we can fire false result in next tick
      Services.DOMRequest.fireSuccessAsync(request, false);
    } else if ((aNumber1 === aNumber2) ||
        (PhoneNumberNormalizer.Normalize(aNumber1) === PhoneNumberNormalizer.Normalize(aNumber2))) {
      // if we have a simple match fire successful request in next tick
      Services.DOMRequest.fireSuccessAsync(request, true);
    } else {
      // invoke fuzzy matching in the parent
      let options = { number1: aNumber1, number2: aNumber2 };
      cpmm.sendAsyncMessage("PhoneNumberService:FuzzyMatch",
                           {requestID: this.getRequestId({request: request}),
                            options: options});
    }

    return request;
  },

  normalize: function(aNumber) {
    if (DEBUG) debug("normalize: " + aNumber);
    return PhoneNumberNormalizer.Normalize(aNumber);
  },

  init: function(aWindow) {
    if (DEBUG) debug("init call");
    this.initDOMRequestHelper(aWindow, [
      "PhoneNumberService:FuzzyMatch:Return:OK",
      "PhoneNumberService:FuzzyMatch:Return:KO"
    ]);
  },

  classID : Components.ID("{e2768710-eb17-11e2-91e2-0800200c9a66}"),
  contractID : "@mozilla.org/phoneNumberService;1",
  QueryInterface : XPCOMUtils.generateQI([Ci.nsIDOMGlobalPropertyInitializer,
                                          Ci.nsISupportsWeakReference,
                                          Ci.nsIObserver]),
}

this.NSGetFactory = XPCOMUtils.generateNSGetFactory([PhoneNumberService]);
