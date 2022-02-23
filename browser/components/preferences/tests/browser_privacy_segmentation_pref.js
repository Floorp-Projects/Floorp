/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

// Test the privacy segmentation pref + telemetry and preferences UI.

"use strict";

const { TelemetryTestUtils } = ChromeUtils.import(
  "resource://testing-common/TelemetryTestUtils.jsm"
);

const PREF = "browser.privacySegmentation.enabled";
const SCALAR_KEY = "browser.privacySegmentation.enabled";

const PREF_VISIBILITY = "browser.privacySegmentation.preferences.show";
const PREF_LEARN_MORE_SUFFIX =
  "browser.privacySegmentation.preferences.learnMoreURLSuffix";

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

add_task(async function test_preferences_section() {
  if (!AppConstants.MOZ_DATA_REPORTING) {
    ok(true, "Skipping test because data reporting is disabled");
    return;
  }

  await openPreferencesViaOpenPreferencesAPI("privacy", { leaveOpen: true });

  let doc = gBrowser.selectedBrowser.contentDocument;
  let section = doc.getElementById("privacySegmentationSection");
  let checkbox = doc.getElementById("privacySegmentationCheck");
  let learnMore = doc.getElementById("privacy-segmentation-learn-more-link");

  for (let show of [false, true]) {
    Services.prefs.setBoolPref(PREF_VISIBILITY, show);
    let showStr = show ? "visible" : "hidden";

    is(
      BrowserTestUtils.is_visible(section),
      show,
      `Privacy Segmentation section should be ${showStr}.`
    );
    is(
      BrowserTestUtils.is_visible(checkbox),
      show,
      `Privacy Segmentation checkbox should be ${showStr}.`
    );
    is(
      BrowserTestUtils.is_visible(learnMore),
      show,
      `Privacy Segmentation learn more label should be ${showStr}.`
    );

    // The section is visible, test checkbox and learn more label.
    if (show) {
      Services.prefs.setBoolPref(PREF, false);

      info("Checking checkbox");
      checkbox.click();
      is(
        Services.prefs.getBoolPref(PREF),
        true,
        "Privacy Segmentation should be enabled."
      );
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

      info("Unchecking checkbox");
      checkbox.click();
      is(
        Services.prefs.getBoolPref(PREF),
        false,
        "Privacy Segmentation should be disabled."
      );
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

      info("Updating pref externally");
      ok(!checkbox.checked, "Checkbox unchecked initially.");
      Services.prefs.setBoolPref(PREF, true);
      await BrowserTestUtils.waitForMutationCondition(
        checkbox,
        { attributeFilter: ["checked"] },
        () => checkbox.checked
      );
      ok(
        checkbox.checked,
        "Updating Privacy Segmentation pref also updates checkbox."
      );
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

      ok(
        learnMore.href.endsWith(
          Services.prefs.getStringPref(PREF_LEARN_MORE_SUFFIX)
        ),
        "learnMore label has href with suffix from pref."
      );
    }
  }

  BrowserTestUtils.removeTab(gBrowser.selectedTab);
  Services.prefs.clearUserPref(PREF_VISIBILITY);
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

add_task(async function test_preferences_section_data_reporting_disabled() {
  if (AppConstants.MOZ_DATA_REPORTING) {
    ok(true, "Skipping test because data reporting is enabled");
    return;
  }

  for (let show of [false, true]) {
    Services.prefs.setBoolPref(PREF_VISIBILITY, show);
    await openPreferencesViaOpenPreferencesAPI("privacy", { leaveOpen: true });

    let doc = gBrowser.selectedBrowser.contentDocument;
    let section = doc.getElementById("privacySegmentationSection");
    is(
      !!section,
      show,
      "Section should only exist when privacy segmentation section is enabled."
    );

    BrowserTestUtils.removeTab(gBrowser.selectedTab);
  }

  Services.prefs.clearUserPref(PREF_VISIBILITY);
});
