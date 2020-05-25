/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

/* global Cc, Ci, ExtensionAPI, TRRRacer  */

ChromeUtils.import("resource://gre/modules/Services.jsm", this);
ChromeUtils.import("resource:///modules/TRRPerformance.jsm", this);

const kDryRunResultPref = "doh-rollout.trr-selection.dry-run-result";

const TRRSELECT_TELEMETRY_CATEGORY = "security.doh.trrPerformance";

Services.telemetry.setEventRecordingEnabled(TRRSELECT_TELEMETRY_CATEGORY, true);

this.trrselect = class trrselect extends ExtensionAPI {
  getAPI() {
    return {
      experiments: {
        trrselect: {
          async dryRun() {
            if (Services.prefs.prefHasUserValue(kDryRunResultPref)) {
              return;
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

            await new Promise(resolve => {
              let racer = new TRRRacer(() => {
                setDryRunResultAndRecordTelemetry(racer.getFastestTRR(true));
                resolve();
              });
              racer.run();
            });
          },
        },
      },
    };
  }
};
