/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict"

const { interfaces: Ci, utils: Cu } = Components;

Cu.import("resource://gre/modules/XPCOMUtils.jsm");
Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource://gre/modules/Promise.jsm");
Cu.import("resource://gre/modules/FxAccountsCommon.js");

XPCOMUtils.defineLazyServiceGetter(this, "uuidgen",
                                   "@mozilla.org/uuid-generator;1",
                                   "nsIUUIDGenerator");

XPCOMUtils.defineLazyModuleGetter(this, "SystemAppProxy",
                                  "resource://gre/modules/SystemAppProxy.jsm");

function FxAccountsUIGlue() {
}

FxAccountsUIGlue.prototype = {

  _contentRequest: function(aEventName, aData) {
    let deferred = Promise.defer();

    let id = uuidgen.generateUUID().toString();

    SystemAppProxy.addEventListener("mozFxAccountsRPContentEvent",
                                    function onContentEvent(result) {
      let msg = result.detail;
      if (!msg || !msg.id || msg.id != id) {
        deferred.reject("InternalErrorWrongContentEvent");
        SystemAppProxy.removeEventListener("mozFxAccountsRPContentEvent",
                                           onContentEvent);
        return;
      }

      log.debug("Got content event " + JSON.stringify(msg));

      if (msg.error) {
        deferred.reject(msg);
      } else {
        deferred.resolve(msg.result);
      }
      SystemAppProxy.removeEventListener("mozFxAccountsRPContentEvent",
                                         onContentEvent);
    });

    let detail = {
       eventName: aEventName,
       id: id,
       data: aData
    };
    log.debug("Send chrome event " + JSON.stringify(detail));
    SystemAppProxy._sendCustomEvent("mozFxAccountsUnsolChromeEvent", detail);

    return deferred.promise;
  },

  signInFlow: function() {
    return this._contentRequest("openFlow");
  },

  refreshAuthentication: function(aAccountId) {
    return this._contentRequest("refreshAuthentication", {
      accountId: aAccountId
    });
  },

  classID: Components.ID("{51875c14-91d7-4b8c-b65d-3549e101228c}"),

  QueryInterface: XPCOMUtils.generateQI([Ci.nsIFxAccountsUIGlue])
};

this.NSGetFactory = XPCOMUtils.generateNSGetFactory([FxAccountsUIGlue]);
