"use strict";

const { ExperimentFakes } = ChromeUtils.import(
  "resource://testing-common/NimbusTestUtils.jsm"
);

/**
 * Enrolls browser in an experiment with value featureValue and
 * runs a test in newtab content
 *
 * @param {string} slug A slug
 * @param {NewTabFeatureConfig} featureValue A feature config
 * @param {NewTabTest} test A new tab test compatible with test_newtab (see head.js)
 */
async function testWithExperimentFeatureValue(slug, featureValue, test) {
  let doExperimentCleanup;
  test_newtab({
    async before() {
      Services.prefs.setBoolPref(
        "browser.newtabpage.activity-stream.newNewtabExperience.enabled",
        false
      );
      doExperimentCleanup = await ExperimentFakes.enrollWithFeatureConfig({
        featureId: "newtab",
        enabled: true,
        value: featureValue,
      });
    },
    test,
    async after() {
      Services.prefs.clearUserPref(
        "browser.newtabpage.activity-stream.newNewtabExperience.enabled"
      );
      await doExperimentCleanup();
    },
  });
}

/**
 * Test the newtab NimbusFeature overrides default values.
 */
testWithExperimentFeatureValue(
  `mochitest-newtab-${Date.now()}`,
  { prefsButtonIcon: "icon-info" },
  async function check_icon_changes_with_experiment() {
    let el = content.document.querySelector(".prefs-button .icon");
    ok(el, "there should be a settings icon");

    await ContentTaskUtils.waitForCondition(
      () =>
        content.document
          .querySelector(".prefs-button .icon")
          .classList.contains("icon-info"),
      "settings icon uses prefsButtonIcon value set in the experiment"
    );
  }
);

/**
 * Test the newtab NimbusFeature uses default values for an empty feature config.
 */
testWithExperimentFeatureValue(
  `mochitest-newtab-${Date.now()}`,
  // Empty feature value should result in using all defaults
  null,
  async function check_icon_uses_default() {
    let el = content.document.querySelector(".prefs-button .icon");
    ok(el, "there should be a settings icon");

    await ContentTaskUtils.waitForCondition(
      () =>
        content.document
          .querySelector(".prefs-button .icon")
          .classList.contains("icon-settings"),
      "settings icon uses default value"
    );
  }
);
