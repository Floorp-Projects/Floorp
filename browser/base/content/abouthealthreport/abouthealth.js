/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const {classes: Cc, interfaces: Ci, utils: Cu} = Components;

Cu.import("resource://gre/modules/Preferences.jsm");
Cu.import("resource://gre/modules/Services.jsm");

const prefs = new Preferences("datareporting.healthreport.");

let healthReportWrapper = {
  init: function () {
    let iframe = document.getElementById("remote-report");
    iframe.addEventListener("load", healthReportWrapper.initRemotePage, false);
    iframe.src = this._getReportURI().spec;
    iframe.onload = () => {
      MozSelfSupport.getHealthReportPayload().then(this.updatePayload,
                                                   this.handleInitFailure);
    };
    prefs.observe("uploadEnabled", this.updatePrefState, healthReportWrapper);
  },

  uninit: function () {
    prefs.ignore("uploadEnabled", this.updatePrefState, healthReportWrapper);
  },

  _getReportURI: function () {
    let url = Services.urlFormatter.formatURLPref("datareporting.healthreport.about.reportUrl");
    return Services.io.newURI(url, null, null);
  },

  setDataSubmission: function (enabled) {
    MozSelfSupport.healthReportDataSubmissionEnabled = enabled;
    this.updatePrefState();
  },

  updatePrefState: function () {
    try {
      let prefs = {
        enabled: MozSelfSupport.healthReportDataSubmissionEnabled,
      };
      healthReportWrapper.injectData("prefs", prefs);
    }
    catch (ex) {
      healthReportWrapper.reportFailure(healthReportWrapper.ERROR_PREFS_FAILED);
    }
  },

  refreshPayload: function () {
    MozSelfSupport.getHealthReportPayload().then(this.updatePayload,
                                                 this.handlePayloadFailure);
  },

  updatePayload: function (payload) {
    healthReportWrapper.injectData("payload", JSON.stringify(payload));
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
        this.setDataSubmission(false);
        break;
      case "EnableDataSubmission":
        this.setDataSubmission(true);
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
