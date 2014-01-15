/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const {classes: Cc, interfaces: Ci, utils: Cu} = Components;

Cu.import("resource://gre/modules/Preferences.jsm");
Cu.import("resource://gre/modules/Services.jsm");

const reporter = Cc["@mozilla.org/datareporting/service;1"]
                   .getService(Ci.nsISupports)
                   .wrappedJSObject
                   .healthReporter;

const policy = Cc["@mozilla.org/datareporting/service;1"]
                 .getService(Ci.nsISupports)
                 .wrappedJSObject
                 .policy;

const prefs = new Preferences("datareporting.healthreport.");


let healthReportWrapper = {
  init: function () {
    if (!reporter) {
      healthReportWrapper.handleInitFailure();
      return;
    }

    reporter.onInit().then(healthReportWrapper.refreshPayload,
                           healthReportWrapper.handleInitFailure);

    let iframe = document.getElementById("remote-report");
    iframe.addEventListener("load", healthReportWrapper.initRemotePage, false);
    let report = this._getReportURI();
    iframe.src = report.spec;
    prefs.observe("uploadEnabled", this.updatePrefState, healthReportWrapper);
  },

  uninit: function () {
    prefs.ignore("uploadEnabled", this.updatePrefState, healthReportWrapper);
  },

  _getReportURI: function () {
    let url = Services.urlFormatter.formatURLPref("datareporting.healthreport.about.reportUrl");
    return Services.io.newURI(url, null, null);
  },

  onOptIn: function () {
    policy.recordHealthReportUploadEnabled(true,
                                           "Health report page sent opt-in command.");
    this.updatePrefState();
  },

  onOptOut: function () {
    policy.recordHealthReportUploadEnabled(false,
                                           "Health report page sent opt-out command.");
    this.updatePrefState();
  },

  updatePrefState: function () {
    try {
      let prefs = {
        enabled: policy.healthReportUploadEnabled,
      }
      this.injectData("prefs", prefs);
    } catch (e) {
      this.reportFailure(this.ERROR_PREFS_FAILED);
    }
  },

  refreshPayload: function () {
    reporter.collectAndObtainJSONPayload().then(healthReportWrapper.updatePayload, 
                                                healthReportWrapper.handlePayloadFailure);
  },

  updatePayload: function (data) {
    healthReportWrapper.injectData("payload", data);
  },

  injectData: function (type, content) {
    let report = this._getReportURI();
    
    // file URIs can't be used for targetOrigin, so we use "*" for this special case
    // in all other cases, pass in the URL to the report so we properly restrict the message dispatch
    let reportUrl = report.scheme == "file" ? "*" : report.spec;

    let data = {
      type: type,
      content: content
    }

    let iframe = document.getElementById("remote-report");
    iframe.contentWindow.postMessage(data, reportUrl);
  },

  handleRemoteCommand: function (evt) {
    switch (evt.detail.command) {
      case "DisableDataSubmission":
        this.onOptOut();
        break;
      case "EnableDataSubmission":
        this.onOptIn();
        break;
      case "RequestCurrentPrefs":
        this.updatePrefState();
        break;
      case "RequestCurrentPayload":
        this.refreshPayload();
        break;
      default:
        Cu.reportError("Unexpected remote command received: " + evt.detail.command + ". Ignoring command.");
        break;
    }
  },

  initRemotePage: function () {
    let iframe = document.getElementById("remote-report").contentDocument;
    iframe.addEventListener("RemoteHealthReportCommand",
                            function onCommand(e) {healthReportWrapper.handleRemoteCommand(e);},
                            false);
    healthReportWrapper.updatePrefState();
  },

  // error handling
  ERROR_INIT_FAILED:    1,
  ERROR_PAYLOAD_FAILED: 2,
  ERROR_PREFS_FAILED:   3,

  reportFailure: function (error) {
    let details = {
      errorType: error,
    }
    healthReportWrapper.injectData("error", details);
  },

  handleInitFailure: function () {
    healthReportWrapper.reportFailure(healthReportWrapper.ERROR_INIT_FAILED);
  },

  handlePayloadFailure: function () {
    healthReportWrapper.reportFailure(healthReportWrapper.ERROR_PAYLOAD_FAILED);
  },
}

window.addEventListener("load", function () { healthReportWrapper.init(); });
window.addEventListener("unload", function () { healthReportWrapper.uninit(); });
