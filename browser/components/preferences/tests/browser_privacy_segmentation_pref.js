/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

// Test the privacy segmentation pref and preferences UI.

"use strict";

const PREF = "browser.dataFeatureRecommendations.enabled";
const PREF_VISIBILITY = "browser.privacySegmentation.preferences.show";

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
      BrowserTestUtils.isVisible(section),
      show,
      `Privacy Segmentation section should be ${showStr}.`
    );
    is(
      BrowserTestUtils.isVisible(sectionHeader),
      show,
      `Privacy Segmentation section header should be ${showStr}.`
    );
    is(
      BrowserTestUtils.isVisible(sectionDescription),
      show,
      `Privacy Segmentation section description should be ${showStr}.`
    );
    is(
      BrowserTestUtils.isVisible(radioGroup),
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

      info("Selecting radio off.");
      radioDisabled.click();
      is(
        Services.prefs.getBoolPref(PREF),
        false,
        "Privacy Segmentation should be disabled."
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
    }
  }

  BrowserTestUtils.removeTab(gBrowser.selectedTab);
  Services.prefs.clearUserPref(PREF_VISIBILITY);
  Services.prefs.clearUserPref(PREF);
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
