/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const {classes: Cc, interfaces: Ci, utils: Cu} = Components;

Cu.import("resource://gre/modules/XPCOMUtils.jsm");

const policy = Cc["@mozilla.org/datareporting/service;1"]
                 .getService(Ci.nsISupports)
                 .wrappedJSObject
                 .policy;

XPCOMUtils.defineLazyGetter(this, "reporter", () => {
  return Cc["@mozilla.org/datareporting/service;1"]
           .getService(Ci.nsISupports)
           .wrappedJSObject
           .healthReporter;
});

function MozSelfSupportInterface() {
}

MozSelfSupportInterface.prototype = {
  classDescription: "MozSelfSupport",
  classID: Components.ID("{d30aae8b-f352-4de3-b936-bb9d875df0bb}"),
  contractID: "@mozilla.org/mozselfsupport;1",
  QueryInterface: XPCOMUtils.generateQI([Ci.nsIDOMGlobalPropertyInitializer,
                                         Ci.nsIObserver]),

  _window: null,

  init: function (window) {
    this._window = window;

    // FIXME: Remove this hack after static attributes work for JS-implemented
    // WebIDL (see bug 863952). For a detailed description of how this works,
    // see the comment accompanying the init function of amInstallTrigger.js.
    return window.MozSelfSupportImpl._create(this._window, this);
  },

  get healthReportDataSubmissionEnabled() {
    return policy.healthReportUploadEnabled;
  },

  set healthReportDataSubmissionEnabled(enabled) {
    let reason = "Self-support interface sent " +
                 (enabled ? "opt-in" : "opt-out") +
                 " command.";
    policy.recordHealthReportUploadEnabled(enabled, reason);
  },

  getHealthReportPayload: function () {
    return new this._window.Promise(function (aResolve, aReject) {
      if (reporter) {
        let resolvePayload = function () {
          reporter.collectAndObtainJSONPayload(true).then(aResolve, aReject);
        };

        if (reporter.initialized) {
          resolvePayload();
        } else {
          reporter.onInit().then(resolvePayload, aReject);
        }
      } else {
        aReject(new Error("No reporter"));
      }
    }.bind(this));
  },
}

this.NSGetFactory = XPCOMUtils.generateNSGetFactory([MozSelfSupportInterface]);
