/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const {classes: Cc, interfaces: Ci, utils: Cu} = Components;

Cu.import("resource://gre/modules/XPCOMUtils.jsm");
Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource://gre/modules/Preferences.jsm");

const PREF_FHR_UPLOAD_ENABLED = "datareporting.healthreport.uploadEnabled";

XPCOMUtils.defineLazyGetter(this, "gPolicy", () => {
  try {
    return Cc["@mozilla.org/datareporting/service;1"]
             .getService(Ci.nsISupports)
             .wrappedJSObject
             .policy;
  } catch (e) {
    return undefined;
  }
});

XPCOMUtils.defineLazyGetter(this, "reporter", () => {
  return Cc["@mozilla.org/datareporting/service;1"]
           .getService(Ci.nsISupports)
           .wrappedJSObject
           .healthReporter;
});

XPCOMUtils.defineLazyModuleGetter(this, "TelemetryArchive",
                                  "resource://gre/modules/TelemetryArchive.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "TelemetryEnvironment",
                                  "resource://gre/modules/TelemetryEnvironment.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "TelemetryController",
                                  "resource://gre/modules/TelemetryController.jsm");

function MozSelfSupportInterface() {
}

MozSelfSupportInterface.prototype = {
  classDescription: "MozSelfSupport",
  classID: Components.ID("{d30aae8b-f352-4de3-b936-bb9d875df0bb}"),
  contractID: "@mozilla.org/mozselfsupport;1",
  QueryInterface: XPCOMUtils.generateQI([Ci.nsIDOMGlobalPropertyInitializer]),

  _window: null,

  init: function (window) {
    this._window = window;
  },

  get healthReportDataSubmissionEnabled() {
    if (gPolicy) {
      return gPolicy.healthReportUploadEnabled;
    }

    // The datareporting service is unavailable or disabled.
    return Preferences.get(PREF_FHR_UPLOAD_ENABLED, false);
  },

  set healthReportDataSubmissionEnabled(enabled) {
    if (gPolicy) {
      let reason = "Self-support interface sent " +
                   (enabled ? "opt-in" : "opt-out") +
                   " command.";
      gPolicy.recordHealthReportUploadEnabled(enabled, reason);
      return;
    }

    // The datareporting service is unavailable or disabled.
    Preferences.set(PREF_FHR_UPLOAD_ENABLED, enabled);
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

  resetPref: function(name) {
    Services.prefs.clearUserPref(name);
  },

  resetSearchEngines: function() {
    Services.search.restoreDefaultEngines();
    Services.search.resetToOriginalDefaultEngine();
  },

  getTelemetryPingList: function() {
    return this._wrapPromise(TelemetryArchive.promiseArchivedPingList());
  },

  getTelemetryPing: function(pingId) {
    return this._wrapPromise(TelemetryArchive.promiseArchivedPingById(pingId));
  },

  getCurrentTelemetryEnvironment: function() {
    const current = TelemetryEnvironment.currentEnvironment;
    return new this._window.Promise(resolve => resolve(current));
  },

  getCurrentTelemetrySubsessionPing: function() {
    const current = TelemetryController.getCurrentPingData(true);
    return new this._window.Promise(resolve => resolve(current));
  },

  _wrapPromise: function(promise) {
    return new this._window.Promise(
      (resolve, reject) => promise.then(resolve, reject));
  },
}

this.NSGetFactory = XPCOMUtils.generateNSGetFactory([MozSelfSupportInterface]);
