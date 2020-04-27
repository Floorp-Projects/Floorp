/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

/* global Cc, Ci, ExtensionAPI  */

ChromeUtils.import("resource://gre/modules/Services.jsm", this);

let pcs = Cc["@mozilla.org/parental-controls-service;1"].getService(
  Ci.nsIParentalControlsService
);

const gDNSService = Cc["@mozilla.org/network/dns-service;1"].getService(
  Ci.nsIDNSService
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

this.heuristics = class heuristics extends ExtensionAPI {
  getAPI() {
    return {
      experiments: {
        heuristics: {
          async isTesting() {
            return Cu.isInAutomation;
          },

          setupTelemetry() {
            // Set up the Telemetry for the heuristics and addon state
            Services.telemetry.registerEvents(
              TELEMETRY_CATEGORY,
              TELEMETRY_EVENTS
            );
          },

          sendHeuristicsPing(decision, results) {
            Services.telemetry.recordEvent(
              TELEMETRY_CATEGORY,
              "evaluate",
              "heuristics",
              decision,
              results
            );
          },

          setDetectedTrrURI(uri) {
            gDNSService.setDetectedTrrURI(uri);
          },

          sendStatePing(state) {
            Services.telemetry.recordEvent(
              TELEMETRY_CATEGORY,
              "state",
              state,
              "null"
            );
          },

          async checkEnterprisePolicies() {
            if (Services.policies.status === Services.policies.ACTIVE) {
              let policies = Services.policies.getActivePolicies();

              if (!policies.hasOwnProperty("DNSOverHTTPS")) {
                // If DoH isn't in the policy, return that there is a policy (but no DoH specifics)
                return "policy_without_doh";
              }

              if (policies.DNSOverHTTPS.Enabled === true) {
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
            if (pcs.parentalControlsEnabled) {
              return "disable_doh";
            }
            return "enable_doh";
          },

          async checkThirdPartyRoots() {
            let certdb = Cc["@mozilla.org/security/x509certdb;1"].getService(
              Ci.nsIX509CertDB
            );

            let allCerts = certdb.getCerts();
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
        },
      },
    };
  }
};
