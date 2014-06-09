/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

this.EXPORTED_SYMBOLS = ["ContentRequestHelper"];

const { interfaces: Ci, utils: Cu } = Components;

Cu.import("resource://gre/modules/XPCOMUtils.jsm");
Cu.import("resource://gre/modules/Promise.jsm");

XPCOMUtils.defineLazyServiceGetter(this, "uuidgen",
                                   "@mozilla.org/uuid-generator;1",
                                   "nsIUUIDGenerator");

XPCOMUtils.defineLazyModuleGetter(this, "SystemAppProxy",
                                  "resource://gre/modules/SystemAppProxy.jsm");

function debug(msg) {
  // dump("ContentRequestHelper ** " + msg + "\n");
}

this.ContentRequestHelper = function() {
}

ContentRequestHelper.prototype = {

  contentRequest: function(aContentEventName, aChromeEventName,
                           aInternalEventName, aData) {
    let deferred = Promise.defer();

    let id = uuidgen.generateUUID().toString();

    SystemAppProxy.addEventListener(aContentEventName,
                                    function onContentEvent(result) {
      SystemAppProxy.removeEventListener(aContentEventName,
                                         onContentEvent);
      let msg = result.detail;
      if (!msg || !msg.id || msg.id != id) {
        deferred.reject("InternalErrorWrongContentEvent " +
                        JSON.stringify(msg));
        SystemAppProxy.removeEventListener(aContentEventName,
                                           onContentEvent);
        return;
      }

      debug("Got content event " + JSON.stringify(msg));

      if (msg.error) {
        deferred.reject(msg.error);
      } else {
        deferred.resolve(msg.result);
      }
    });

    let detail = {
       eventName: aInternalEventName,
       id: id,
       data: aData
    };
    debug("Send chrome event " + JSON.stringify(detail));
    SystemAppProxy._sendCustomEvent(aChromeEventName, detail);

    return deferred.promise;
  }
};
