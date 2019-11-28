/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";
/* exported heuristics */
/* global Cc, Ci, ExtensionAPI  */
ChromeUtils.import("resource://gre/modules/Services.jsm", this);

function log() {
  // eslint-disable-next-line no-constant-condition
  if (false) {
    // eslint-disable-next-line no-console
    console.log(...arguments);
  }
}

let pcs = Cc["@mozilla.org/parental-controls-service;1"].getService(
  Ci.nsIParentalControlsService
);

const TELEMETRY_CATEGORY = "doh";

const TELEMETRY_EVENTS = {
  evaluate: {
    methods: ["evaluate"],
    objects: ["heuristics"],
    extra_keys: [
      "google",
      "youtube",
      "zscalerCanary",
      "canary",
      "modifiedRoots",
      "browserParent",
      "thirdPartyRoots",
      "policy",
      "evaluateReason",
    ],
    record_on_release: true,
  },
  state: {
    methods: ["state"],
    objects: [
      "loaded",
      "enabled",
      "disabled",
      "manuallyDisabled",
      "uninstalled",
      "UIOk",
      "UIDisabled",
    ],
    extra_keys: [],
    record_on_release: true,
  },
};

const heuristicsManager = {
  setupTelemetry() {
    // Set up the Telemetry for the heuristics  and addon state
    Services.telemetry.registerEvents(TELEMETRY_CATEGORY, TELEMETRY_EVENTS);
  },

  sendHeuristicsPing(decision, results) {
    log("Sending a heuristics ping", decision, results);
    Services.telemetry.recordEvent(
      TELEMETRY_CATEGORY,
      "evaluate",
      "heuristics",
      decision,
      results
    );
  },

  sendStatePing(state) {
    log("Sending an addon state ping", state);
    Services.telemetry.recordEvent(TELEMETRY_CATEGORY, "state", state, "null");
  },

  async checkEnterprisePolicies() {
    if (Services.policies.status === Services.policies.ACTIVE) {
      let policies = Services.policies.getActivePolicies();
      if (!("DNSOverHTTPS" in policies)) {
        // If DoH isn't in the policy, return that there is a policy (but no DoH specifics)
        return "policy_without_doh";
      }
      let dohPolicy = policies.DNSOverHTTPS;
      if (dohPolicy.Enabled === true) {
        // If DoH is enabled in the policy, enable it
        return "enable_doh";
      }
      // If DoH is disabled in the policy, disable it
      return "disable_doh";
    }

    // Default return, meaning no policy related to DNSOverHTTPS
    return "no_policy_set";
  },

  async checkParentalControls() {
    let enabled = pcs.parentalControlsEnabled;
    if (enabled) {
      return "disable_doh";
    }
    return "enable_doh";
  },

  async checkThirdPartyRoots() {
    let certdb = Cc["@mozilla.org/security/x509certdb;1"].getService(
      Ci.nsIX509CertDB
    );
    let allCerts = certdb.getCerts();
    // Pre 71 code (Bug 1577836)
    if (allCerts.getEnumerator) {
      allCerts = allCerts.getEnumerator();
    }
    for (let cert of allCerts) {
      if (
        certdb.isCertTrusted(
          cert,
          Ci.nsIX509Cert.CA_CERT,
          Ci.nsIX509CertDB.TRUSTED_SSL
        )
      ) {
        if (!cert.isBuiltInRoot) {
          // this cert is a trust anchor that wasn't shipped with the browser
          return "disable_doh";
        }
      }
    }
    return "enable_doh";
  },
};

var heuristics = class heuristics extends ExtensionAPI {
  getAPI() {
    return {
      experiments: {
        heuristics: {
          setupTelemetry() {
            heuristicsManager.setupTelemetry();
          },

          sendHeuristicsPing(decision, results) {
            heuristicsManager.sendHeuristicsPing(decision, results);
          },

          sendStatePing(state) {
            heuristicsManager.sendStatePing(state);
          },

          async checkEnterprisePolicies() {
            let result = await heuristicsManager.checkEnterprisePolicies();
            return result;
          },

          async checkParentalControls() {
            let result = await heuristicsManager.checkParentalControls();
            return result;
          },

          async checkThirdPartyRoots() {
            let result = await heuristicsManager.checkThirdPartyRoots();
            return result;
          },
        },
      },
    };
  }
};
