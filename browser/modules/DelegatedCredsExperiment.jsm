/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

var EXPORTED_SYMBOLS = ["DelegatedCredsExperiment"];

const { Services } = ChromeUtils.import("resource://gre/modules/Services.jsm");

// Test constants
const kBranchControl = "control";
const kBranchTreatment = "treatment";
const kDelegatedCredentialsHost = "enabled.dc.crypto.mozilla.org";
const kDelegatedCredentialsPref = "security.tls.enable_delegated_credentials";

// Prefs
const kExperimentHasRun = "dc-experiment.hasRun";
const kForceTreatment = "dc-experiment.branchTreatment";
const kTargetHost = "dc-experiment.host";
const kPreviousPrefPrefix = "dc-experiment.previous.";

// Telemetry
const kTelemetryCategory = "delegatedcredentials"; // Can't have an underscore :(

const kTelemetryEvents = {
  experiment: {
    methods: ["connectDC", "connectNoDC"],
    objects: [
      "success",
      "timedOut",
      "hsNotDelegated",
      "certNotDelegated",
      "dnsFailure",
      "networkFailure",
      "insufficientSecurity",
      "incorrectTLSVersion",
    ],
  },
};

const kResults = {
  SUCCESS: "success",
  TIMEOUT: "timedOut",
  SUCCESS_NO_DC: "hsNotDelegated",
  CERT_NO_DC: "certNotDelegated",
  DNS_FAILURE: "dnsFailure",
  NET_FAILURE: "networkFailure",
  INSUFFICIENT_SECURITY: "insufficientSecurity",
  INCORRECT_TLS_VERSION: "incorrectTLSVersion",
};

/* Prefs handlers */
const prefManager = {
  prefHasUserValue(name) {
    return Services.prefs.prefHasUserValue(name);
  },

  getBoolPref(name, value) {
    return Services.prefs.getBoolPref(name, value);
  },

  setBoolPref(name, value) {
    Services.prefs.setBoolPref(name, value);
  },

  getStringPref(name, value) {
    return Services.prefs.getStringPref(name, value);
  },

  rememberBoolPref(name) {
    let curMode = Services.prefs.getBoolPref(name);
    Services.prefs.setBoolPref(kPreviousPrefPrefix + name, curMode);
  },

  restoreBoolPref(name) {
    let prevMode = Services.prefs.getBoolPref(kPreviousPrefPrefix + name);
    Services.prefs.setBoolPref(name, prevMode);
    Services.prefs.clearUserPref(kPreviousPrefPrefix + name);
  },
};

function setResult(result, telemetryResult) {
  result.telemetryResult = telemetryResult;
  result.hasResult = true; // Report it and mark the experiment complete.
}

/* Record one of the following for telemetry:
 * |success|: Connected successfully using a delegated credential.
 * |handshakeNotDelegated|: Connected successfully, but did not negotiate using delegated credential.
 * |certificateNotDelegated|: Connected successfully, but the certificate did not permit delegated credentials.
 * |timedOut|: Network timeout.
 * |dnsFailure|: Failed to connect due to a DNS failure.
 * |networkFailure|: Failed to connect due to a non-timeout, non-dns network error (connection reset, etc).
 * |insufficientSecurity|: The delegated credential did not provide high enough security.
 * |incorrectTLSVersion|: Connected successfully, but used TLS < 1.3. */
function populateResult(channel, result) {
  let secInfo = channel.securityInfo;
  if (secInfo instanceof Ci.nsITransportSecurityInfo) {
    let isSecure =
      (secInfo.securityState & Ci.nsIWebProgressListener.STATE_IS_SECURE) ==
      Ci.nsIWebProgressListener.STATE_IS_SECURE;

    if (result.status >= 400 && result.status < 521) {
      // HTTP Error codes indicating network error.
      setResult(result, kResults.NET_FAILURE);
      if (result.status == 408) {
        setResult(result, kResults.TIMEOUT); // Except this one.
      }
    } else if (isSecure && (result.status == 0 || result.status == 200)) {
      if (secInfo.protocolVersion < secInfo.TLS_VERSION_1_3) {
        setResult(result, kResults.INCORRECT_TLS_VERSION);
      } else if (!secInfo.isDelegatedCredential) {
        setResult(result, kResults.SUCCESS_NO_DC);
      } else if (secInfo.isDelegatedCredential) {
        setResult(result, kResults.SUCCESS);
      }
    } else {
      const MOZILLA_PKIX_ERROR_INADEQUATE_KEY_SIZE = 2153398270;
      const SSL_ERROR_DC_INVALID_KEY_USAGE = 2153393992;
      if (result.nsiReqError == MOZILLA_PKIX_ERROR_INADEQUATE_KEY_SIZE) {
        // DC key strength was too weak
        setResult(result, kResults.INSUFFICIENT_SECURITY);
      } else if (result.nsiReqError == SSL_ERROR_DC_INVALID_KEY_USAGE) {
        // Certificate did not contain the DC extension
        setResult(result, kResults.CERT_NO_DC);
      }
    }
  } else {
    switch (result.nsiReqError) {
      case Cr.NS_ERROR_UNKNOWN_HOST:
        setResult(result, kResults.DNS_FAILURE);
        break;
      default:
        // Default to NET_FAILURE as there are many potential causes.
        setResult(result, kResults.NET_FAILURE);
        break;
    }
  }
  // The default is to leave hasResult unset and repeat the test.
}

/* Submit the result for telemetry, and return true if successful.
 * If the telemetry submission was unsuccessful OR the result itself
 * indicates that we should retry the experiment, return false. */
function recordResult(result) {
  if (result.status === 521 || !result.hasResult) {
    // 521 result means we could reach CF, but CF could not reach the host. In this case,
    // mark the experiment as not-run, allowing it run again.
    return false;
  }
  Services.telemetry.recordEvent(
    kTelemetryCategory,
    result.method,
    result.telemetryResult
  );
  return true;
}

function finishExperiment(result) {
  // Revert the DC setting
  prefManager.restoreBoolPref(kDelegatedCredentialsPref);

  if (result.hasResult && recordResult(result)) {
    // Mark the experiment as completed.
    prefManager.setBoolPref(kExperimentHasRun, true);
  }
}

function makeRequest(branch) {
  var result = {
    method: branch == kBranchControl ? "connectNoDC" : "connectDC",
    hasResult: false, // True when we have something worth reporting
  };

  var oReq = new XMLHttpRequest();
  let url =
    "https://" +
    prefManager.getStringPref(kTargetHost, kDelegatedCredentialsHost);
  oReq.open("HEAD", url + (/\?/.test(url) ? "&" : "?") + new Date().getTime());
  oReq.setRequestHeader(
    "X-Firefox-Experiment",
    "Delegated Credentials Breakage #1; https://bugzilla.mozilla.org/show_bug.cgi?id=1628012"
  );
  oReq.timeout = 30000;
  oReq.addEventListener("error", e => {
    let channel = e.target.channel;
    let nsireq = channel.QueryInterface(Ci.nsIRequest);
    result.nsiReqError = nsireq ? nsireq.status : Cr.NS_ERROR_NOT_AVAILABLE;
    populateResult(channel, result);
    finishExperiment(result);
  });
  oReq.addEventListener("load", e => {
    result.status = e.target.status;
    let nsireq = e.target.channel.QueryInterface(Ci.nsIRequest);
    result.nsiReqError = nsireq.status;
    populateResult(e.target.channel, result);
    finishExperiment(result);
  });
  oReq.addEventListener("timeout", () => {
    setResult(result, kResults.TIMEOUT);
    finishExperiment(result);
  });
  oReq.addEventListener("abort", () => {
    // Will retry
    finishExperiment(result);
  });

  oReq.send();
}

function getEnrollmentStatus() {
  let hasRun = prefManager.getBoolPref(kExperimentHasRun, false);
  return !hasRun;
}

// Returns true iff the test is to be performed with DC enabled.
function getDCTreatment() {
  return Math.random() >= 0.5;
}

const DelegatedCredsExperiment = {
  uninstall() {
    // Cleanup our study prefs
    Services.prefs.clearUserPref(kExperimentHasRun);
    Services.prefs.clearUserPref(
      kPreviousPrefPrefix + kDelegatedCredentialsPref
    );
  },

  runTest() {
    // If the the addon stored a previous setting for "enabled Delegated Credentials" and it still exists,
    // then the test didn't exit/cleanup proplerly. In this case, restore the setting and mark the test completed.
    if (
      prefManager.prefHasUserValue(
        kPreviousPrefPrefix + kDelegatedCredentialsPref
      )
    ) {
      prefManager.restoreBoolPref(kDelegatedCredentialsPref);
      prefManager.setBoolPref(kExperimentHasRun, true);
      return;
    }

    // If the user has already changed the default setting or they are not randomly selected, return early.
    if (
      prefManager.prefHasUserValue(kDelegatedCredentialsPref) ||
      !getEnrollmentStatus()
    ) {
      prefManager.setBoolPref(kExperimentHasRun, true);
      return;
    }

    prefManager.rememberBoolPref(kDelegatedCredentialsPref);

    // Get the value of dc-experiment.branch if it exists, otherwise choose randomly.
    let testBranch = prefManager.getBoolPref(kForceTreatment, getDCTreatment())
      ? kBranchTreatment
      : kBranchControl;

    if (testBranch === kBranchTreatment) {
      prefManager.setBoolPref(kDelegatedCredentialsPref, true);
    } else {
      prefManager.setBoolPref(kDelegatedCredentialsPref, false);
    }

    Services.telemetry.registerEvents(kTelemetryCategory, kTelemetryEvents);
    makeRequest(testBranch);
  },
};
