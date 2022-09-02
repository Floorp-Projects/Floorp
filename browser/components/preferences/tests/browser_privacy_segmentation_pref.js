/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

// Test the privacy segmentation pref + telemetry and preferences UI.

"use strict";

const { TelemetryTestUtils } = ChromeUtils.import(
  "resource://testing-common/TelemetryTestUtils.jsm"
);

const PREF = "browser.dataFeatureRecommendations.enabled";
const PREF_VISIBILITY = "browser.privacySegmentation.preferences.show";

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
  let sectionHeader = section.querySelector("h2");
  let sectionDescription = section.querySelector("label");
  let radioGroup = section.querySelector(
    "#privacyDataFeatureRecommendationRadioGroup"
  );
  let radioEnabled = radioGroup.querySelector(
    "#privacyDataFeatureRecommendationEnabled"
  );
  let radioDisabled = radioGroup.querySelector(
    "#privacyDataFeatureRecommendationDisabled"
  );

  for (let show of [false, true]) {
    Services.prefs.setBoolPref(PREF_VISIBILITY, show);
    let showStr = show ? "visible" : "hidden";

    is(
      BrowserTestUtils.is_visible(section),
      show,
      `Privacy Segmentation section should be ${showStr}.`
    );
    is(
      BrowserTestUtils.is_visible(sectionHeader),
      show,
      `Privacy Segmentation section header should be ${showStr}.`
    );
    is(
      BrowserTestUtils.is_visible(sectionDescription),
      show,
      `Privacy Segmentation section description should be ${showStr}.`
    );
    is(
      BrowserTestUtils.is_visible(radioGroup),
      show,
      `Privacy Segmentation radio group should be ${showStr}.`
    );

    // The section is visible, test radio buttons.
    if (show) {
      Services.prefs.setBoolPref(PREF, false);

      is(
        radioGroup.value,
        "false",
        "Radio group should reflect initial pref state of false."
      );

      info("Selecting radio on.");
      radioEnabled.click();
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

      info("Selecting radio off.");
      radioDisabled.click();
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
      is(
        radioGroup.value,
        "false",
        "Radio group should reflect initial pref state of false."
      );
      Services.prefs.setBoolPref(PREF, true);
      await BrowserTestUtils.waitForMutationCondition(
        radioGroup,
        { attributeFilter: ["value"] },
        () => radioGroup.value == "true"
      );
      is(
        radioGroup.value,
        "true",
        "Updating Privacy Segmentation pref also updates radio group."
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
