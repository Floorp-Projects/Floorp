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

function FxAccountsUIGlue() {
}

FxAccountsUIGlue.prototype = {

  _browser: Services.wm.getMostRecentWindow("navigator:browser"),

  signInFlow: function() {
    let deferred = Promise.defer();

    let content = this._browser.getContentWindow();
    if (!content) {
      deferred.reject("InternalErrorNoContent");
      return;
    }

    let id = uuidgen.generateUUID().toString();

    content.addEventListener("mozFxAccountsRPContentEvent",
                             function onContentEvent(result) {
      let msg = result.detail;
      if (!msg || !msg.id || msg.id != id) {
        deferred.reject("InternalErrorWrongContentEvent");
        content.removeEventListener("mozFxAccountsRPContentEvent",
                                    onContentEvent);
        return;
      }

      log.debug("Got content event " + JSON.stringify(msg));

      if (msg.error) {
        deferred.reject(msg);
      } else {
        deferred.resolve(msg.result);
      }
      content.removeEventListener("mozFxAccountsRPContentEvent",
                                  onContentEvent);
    });

    let detail = {
       eventName: "openFlow",
       id: id
    };
    log.debug("Send chrome event " + JSON.stringify(detail));
    this._browser.shell.sendCustomEvent("mozFxAccountsUnsolChromeEvent", detail);

    return deferred.promise;
  },

  classID: Components.ID("{51875c14-91d7-4b8c-b65d-3549e101228c}"),

  QueryInterface: XPCOMUtils.generateQI([Ci.nsIFxAccountsUIGlue])
};

this.NSGetFactory = XPCOMUtils.generateNSGetFactory([FxAccountsUIGlue]);
