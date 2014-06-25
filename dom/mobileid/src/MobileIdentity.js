/* -*- js-indent-level: 2; indent-tabs-mode: nil -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict"

const {classes: Cc, interfaces: Ci, utils: Cu} = Components;

Cu.import("resource://gre/modules/DOMRequestHelper.jsm");
Cu.import("resource://gre/modules/MobileIdentityCommon.jsm");
Cu.import("resource://gre/modules/XPCOMUtils.jsm");

const MOBILEIDSERVICE_CID =
  Components.ID("{6ec1806c-d61f-4724-9495-68c26d46dc53}");

const IPC_MSG_NAMES = ["MobileId:GetAssertion:Return:OK",
                       "MobileId:GetAssertion:Return:KO"];

XPCOMUtils.defineLazyServiceGetter(this, "cpmm",
                                   "@mozilla.org/childprocessmessagemanager;1",
                                   "nsIMessageSender");

function MobileIdentityService() {
}

MobileIdentityService.prototype = {
  __proto__: DOMRequestIpcHelper.prototype,

  // TODO: this should be handled by DOMRequestIpcHelper. Bug 1020582
  _windows: {},

  getMobileIdAssertion: function(aWindow) {
    log.debug("getMobileIdAssertion");

    if (!this.init) {
      this.initDOMRequestHelper(aWindow, IPC_MSG_NAMES);
      this.init = true;
    }

    return new aWindow.Promise(
      (resolve, reject) => {
        let promiseId = this.getPromiseResolverId({
          resolve: resolve,
          reject: reject
        });

        this._windows[promiseId] = aWindow;

        cpmm.sendAsyncMessage("MobileId:GetAssertion", {
          promiseId: promiseId
        }, null, aWindow.document.nodePrincipal);
      }
    );
  },

  receiveMessage: function(aMessage) {
    let name = aMessage.name;
    let msg = aMessage.json;

    log.debug("Received message " + name + ": " + JSON.stringify(msg));

    let promiseId = msg.promiseId || msg.requestID;
    let promise = this.takePromiseResolver(promiseId);
    if (!promise) {
      log.error("Promise not found");
      return;
    }

    let _window = this._windows[promiseId];
    delete this._windows[promiseId];
    if (!_window) {
      log.error("No window object found");
      return;
    }

    switch (name) {
      case "MobileId:GetAssertion:Return:OK":
        if (!msg.result) {
          promise.reject(new _window.DOMError(ERROR_INTERNAL_UNEXPECTED));
        }

        // Return the assertion
        promise.resolve(msg.result);
        break;
      case "MobileId:GetAssertion:Return:KO":
        promise.reject(new _window.DOMError(msg.error || ERROR_UNKNOWN));
        break;
    }
  },

  classID: MOBILEIDSERVICE_CID,

  QueryInterface: XPCOMUtils.generateQI([Ci.nsIMobileIdentityService,
                                         Ci.nsISupportsWeakReference,
                                         Ci.nsIObserver]),

  classInfo: XPCOMUtils.generateCI({
    classID: MOBILEIDSERVICE_CID,
    contractID: "@mozilla.org/mobileidentity-service;1",
    interfaces: [Ci.nsIMobileIdentityService],
    flags: Ci.nsIClassInfo.SINGLETON
  })

};

this.NSGetFactory = XPCOMUtils.generateNSGetFactory([MobileIdentityService]);
