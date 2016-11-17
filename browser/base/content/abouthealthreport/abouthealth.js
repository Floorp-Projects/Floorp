/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

var {classes: Cc, interfaces: Ci, utils: Cu} = Components;

Cu.import("resource://gre/modules/Preferences.jsm");
Cu.import("resource://gre/modules/Services.jsm");

const prefs = new Preferences("datareporting.healthreport.");

const PREF_UNIFIED = "toolkit.telemetry.unified";
const PREF_REPORTING_URL = "datareporting.healthreport.about.reportUrl";

var healthReportWrapper = {
  init: function() {
    let iframe = document.getElementById("remote-report");
    iframe.addEventListener("load", healthReportWrapper.initRemotePage, false);
    iframe.src = this._getReportURI().spec;
    prefs.observe("uploadEnabled", this.updatePrefState, healthReportWrapper);
  },

  uninit: function() {
    prefs.ignore("uploadEnabled", this.updatePrefState, healthReportWrapper);
  },

  _getReportURI: function() {
    let url = Services.urlFormatter.formatURLPref(PREF_REPORTING_URL);
    return Services.io.newURI(url, null, null);
  },

  setDataSubmission: function(enabled) {
    MozSelfSupport.healthReportDataSubmissionEnabled = enabled;
    this.updatePrefState();
  },

  updatePrefState: function() {
    try {
      let prefsObj = {
        enabled: MozSelfSupport.healthReportDataSubmissionEnabled,
      };
      healthReportWrapper.injectData("prefs", prefsObj);
    }
    catch (ex) {
      healthReportWrapper.reportFailure(healthReportWrapper.ERROR_PREFS_FAILED);
    }
  },

  sendTelemetryPingList: function() {
    console.log("AboutHealthReport: Collecting Telemetry ping list.");
    MozSelfSupport.getTelemetryPingList().then((list) => {
      console.log("AboutHealthReport: Sending Telemetry ping list.");
      this.injectData("telemetry-ping-list", list);
    }).catch((ex) => {
      console.log("AboutHealthReport: Collecting ping list failed: " + ex);
    });
  },

  sendTelemetryPingData: function(pingId) {
    console.log("AboutHealthReport: Collecting Telemetry ping data.");
    MozSelfSupport.getTelemetryPing(pingId).then((ping) => {
      console.log("AboutHealthReport: Sending Telemetry ping data.");
      this.injectData("telemetry-ping-data", {
        id: pingId,
        pingData: ping,
      });
    }).catch((ex) => {
      console.log("AboutHealthReport: Loading ping data failed: " + ex);
      this.injectData("telemetry-ping-data", {
        id: pingId,
        error: "error-generic",
      });
    });
  },

  sendCurrentEnvironment: function() {
    console.log("AboutHealthReport: Sending Telemetry environment data.");
    MozSelfSupport.getCurrentTelemetryEnvironment().then((environment) => {
      this.injectData("telemetry-current-environment-data", environment);
    }).catch((ex) => {
      console.log("AboutHealthReport: Collecting current environment data failed: " + ex);
    });
  },

  sendCurrentPingData: function() {
    console.log("AboutHealthReport: Sending current Telemetry ping data.");
    MozSelfSupport.getCurrentTelemetrySubsessionPing().then((ping) => {
      this.injectData("telemetry-current-ping-data", ping);
    }).catch((ex) => {
      console.log("AboutHealthReport: Collecting current ping data failed: " + ex);
    });
  },

  injectData: function(type, content) {
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

  handleRemoteCommand: function(evt) {
    // Do an origin check to harden against the frame content being loaded from unexpected locations.
    let allowedPrincipal = Services.scriptSecurityManager.getCodebasePrincipal(this._getReportURI());
    let targetPrincipal = evt.target.nodePrincipal;
    if (!allowedPrincipal.equals(targetPrincipal)) {
      Cu.reportError(`Origin check failed for message "${evt.detail.command}": ` +
                     `target origin is "${targetPrincipal.origin}", expected "${allowedPrincipal.origin}"`);
      return;
    }

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
      case "RequestTelemetryPingList":
        this.sendTelemetryPingList();
        break;
      case "RequestTelemetryPingData":
        this.sendTelemetryPingData(evt.detail.id);
        break;
      case "RequestCurrentEnvironment":
        this.sendCurrentEnvironment();
        break;
      case "RequestCurrentPingData":
        this.sendCurrentPingData();
        break;
      default:
        Cu.reportError("Unexpected remote command received: " + evt.detail.command + ". Ignoring command.");
        break;
    }
  },

  initRemotePage: function() {
    let iframe = document.getElementById("remote-report").contentDocument;
    iframe.addEventListener("RemoteHealthReportCommand",
                            function onCommand(e) { healthReportWrapper.handleRemoteCommand(e); },
                            false);
    healthReportWrapper.updatePrefState();
  },

  // error handling
  ERROR_INIT_FAILED:    1,
  ERROR_PAYLOAD_FAILED: 2,
  ERROR_PREFS_FAILED:   3,

  reportFailure: function(error) {
    let details = {
      errorType: error,
    }
    healthReportWrapper.injectData("error", details);
  },

  handleInitFailure: function() {
    healthReportWrapper.reportFailure(healthReportWrapper.ERROR_INIT_FAILED);
  },

  handlePayloadFailure: function() {
    healthReportWrapper.reportFailure(healthReportWrapper.ERROR_PAYLOAD_FAILED);
  },
}

window.addEventListener("load", function() { healthReportWrapper.init(); });
window.addEventListener("unload", function() { healthReportWrapper.uninit(); });
