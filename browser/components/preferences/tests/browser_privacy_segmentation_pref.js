/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

// Test the privacy segmentation pref + telemetry.

"use strict";

const { TelemetryTestUtils } = ChromeUtils.import(
  "resource://testing-common/TelemetryTestUtils.jsm"
);

const PREF = "browser.privacySegmentation.enabled";
const SCALAR_KEY = "browser.privacySegmentation.enabled";

add_task(async function test_telemetry() {
  ok(
    !Services.prefs.prefHasUserValue(PREF),
    `Pref '${PREF}' should not have user value initially.'`
  );
  let prefValue = Services.prefs.getBoolPref(PREF);
  ok(!prefValue, `'${PREF}' should be disabled initially.`);

  TelemetryTestUtils.assertEvents([], {
    category: "privacy_segmentation",
    object: "pref",
  });

  info("Enable privacy segmentation");
  Services.prefs.setBoolPref(PREF, true);
  prefValue = Services.prefs.getBoolPref(PREF);
  ok(prefValue, `'${PREF}' should be enabled.`);

  TelemetryTestUtils.assertEvents(
    [["privacy_segmentation", "enable", "pref"]],
    {
      category: "privacy_segmentation",
      object: "pref",
    },
    {
      clear: false,
    }
  );

  info("Disable privacy segmentation");
  Services.prefs.setBoolPref(PREF, false);
  prefValue = Services.prefs.getBoolPref(PREF);
  ok(!prefValue, `'${PREF}' should be disabled.`);

  TelemetryTestUtils.assertEvents(
    [
      ["privacy_segmentation", "enable", "pref"],
      ["privacy_segmentation", "disable", "pref"],
    ],
    {
      category: "privacy_segmentation",
      object: "pref",
    },
    {
      clear: false,
    }
  );

  info("Re-enable privacy segmentation");
  Services.prefs.setBoolPref(PREF, true);
  prefValue = Services.prefs.getBoolPref(PREF);
  ok(prefValue, `'${PREF}' should be enabled.`);

  TelemetryTestUtils.assertEvents(
    [
      ["privacy_segmentation", "enable", "pref"],
      ["privacy_segmentation", "disable", "pref"],
      ["privacy_segmentation", "enable", "pref"],
    ],
    {
      category: "privacy_segmentation",
      object: "pref",
    },
    {
      clear: true,
    }
  );

  Services.prefs.clearUserPref(PREF);
  TelemetryTestUtils.assertEvents(
    [["privacy_segmentation", "disable", "pref"]],
    {
      category: "privacy_segmentation",
      object: "pref",
    },
    {
      clear: true,
    }
  );
});
