/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

/* global Cc, Ci, ExtensionAPI, TRRRacer  */

ChromeUtils.import("resource://gre/modules/Services.jsm", this);

const kEnabledPref = "doh-rollout.trr-selection.enabled";
const kCommitSelectionPref = "doh-rollout.trr-selection.commit-result";
const kDryRunResultPref = "doh-rollout.trr-selection.dry-run-result";
const kRolloutURIPref = "doh-rollout.uri";
const kTRRListPref = "network.trr.resolvers";

const TRRSELECT_TELEMETRY_CATEGORY = "security.doh.trrPerformance";

Services.telemetry.setEventRecordingEnabled(TRRSELECT_TELEMETRY_CATEGORY, true);

this.trrselect = class trrselect extends ExtensionAPI {
  getAPI() {
    return {
      experiments: {
        trrselect: {
          async dryRun() {
            if (Services.prefs.prefHasUserValue(kDryRunResultPref)) {
              // Check whether the existing dry-run-result is in the default
              // list of TRRs. If it is, all good. Else, run the dry run again.
              let dryRunResult = Services.prefs.getCharPref(kDryRunResultPref);
              let defaultTRRs = JSON.parse(
                Services.prefs.getDefaultBranch("").getCharPref(kTRRListPref)
              );
              let dryRunResultIsValid = defaultTRRs.some(
                trr => trr.url == dryRunResult
              );
              if (dryRunResultIsValid) {
                return;
              }
            }

            let setDryRunResultAndRecordTelemetry = trr => {
              Services.prefs.setCharPref(kDryRunResultPref, trr);
              Services.telemetry.recordEvent(
                TRRSELECT_TELEMETRY_CATEGORY,
                "trrselect",
                "dryrunresult",
                trr.substring(0, 40) // Telemetry payload max length
              );
            };

            if (Cu.isInAutomation) {
              // For mochitests, just record telemetry with a dummy result.
              // TRRPerformance.jsm is tested in xpcshell.
              setDryRunResultAndRecordTelemetry("dummyTRR");
              return;
            }

            // Importing the module here saves us from having to do it at add-on
            // startup, and ensures tests have time to set prefs before the
            // module initializes.
            let { TRRRacer } = ChromeUtils.import(
              "resource:///modules/TRRPerformance.jsm"
            );
            await new Promise(resolve => {
              let racer = new TRRRacer(() => {
                setDryRunResultAndRecordTelemetry(racer.getFastestTRR(true));
                resolve();
              });
              racer.run();
            });
          },

          async run() {
            if (!Services.prefs.getBoolPref(kEnabledPref, false)) {
              return;
            }

            if (Services.prefs.prefHasUserValue(kRolloutURIPref)) {
              return;
            }

            await this.dryRun();

            // If persisting the selection is disabled, don't commit the value.
            if (!Services.prefs.getBoolPref(kCommitSelectionPref, false)) {
              return;
            }

            Services.prefs.setCharPref(
              kRolloutURIPref,
              Services.prefs.getCharPref(kDryRunResultPref)
            );
          },
        },
      },
    };
  }
};
