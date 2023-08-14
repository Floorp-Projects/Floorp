/* Any copyright is dedicated to the Public Domain.
 * https://creativecommons.org/publicdomain/zero/1.0/ */

// This file tests the Privacy pane's Cookie Banner Handling UI.

"use strict";

const FEATURE_PREF = "privacy.globalprivacycontrol.functionality.enabled";
const MODE_PREF = "privacy.globalprivacycontrol.enabled";
const DNT_PREF = "privacy.donottrackheader.enabled";

const SECTION_ID = "nonTechnicalPrivacyBox";
const GPC_ID = "globalPrivacyControlBox";
const GPC_CHECKBOX_ID = "globalPrivacyControlCheckbox";
const OLD_DNT_ID = "legacyDoNotTrackBox";
const NEW_DNT_ID = "doNotTrackBox";
const DNT_CHECKBOX_ID = "doNotTrackCheckbox";

// Test the section is hidden on page load if the feature pref is disabled.
// Also make sure we keep the old DNT interface.
add_task(async function test_section_hidden_when_feature_flag_disabled() {
  await SpecialPowers.pushPrefEnv({
    set: [
      [FEATURE_PREF, false],
      [MODE_PREF, false],
    ],
  });

  await BrowserTestUtils.withNewTab(
    { gBrowser, url: "about:preferences#privacy" },
    async function (browser) {
      let gpc = browser.contentDocument.getElementById(GPC_ID);
      is_element_hidden(gpc, "#globalPrivacyControlBox is hidden");
      let new_dnt = browser.contentDocument.getElementById(NEW_DNT_ID);
      is_element_hidden(new_dnt, "#doNotTrackBox is hidden");
      let old_dnt = browser.contentDocument.getElementById(OLD_DNT_ID);
      is_element_visible(old_dnt, "#doNotTrackBox is shown");
    }
  );

  await SpecialPowers.popPrefEnv();
});

// Test the section is shown on page load if the feature pref is enabled.
// Also make sure we show the new DNT interface.
add_task(async function test_section_shown_when_feature_flag_enabled() {
  await SpecialPowers.pushPrefEnv({
    set: [
      [FEATURE_PREF, true],
      [MODE_PREF, false],
    ],
  });

  await BrowserTestUtils.withNewTab(
    { gBrowser, url: "about:preferences#privacy" },
    async function (browser) {
      let gpc = browser.contentDocument.getElementById(GPC_ID);
      is_element_visible(gpc, "#globalPrivacyControlBox is shown");
      let new_dnt = browser.contentDocument.getElementById(NEW_DNT_ID);
      is_element_visible(new_dnt, "#doNotTrackBox is shown");
      let old_dnt = browser.contentDocument.getElementById(OLD_DNT_ID);
      is_element_hidden(old_dnt, "#doNotTrackBox is hidden");
    }
  );

  await SpecialPowers.popPrefEnv();
});

// Test the checkbox is unchecked in DISABLED mode.
add_task(async function test_checkbox_unchecked_disabled_mode() {
  await SpecialPowers.pushPrefEnv({
    set: [
      [FEATURE_PREF, true],
      [MODE_PREF, false],
    ],
  });

  await BrowserTestUtils.withNewTab(
    { gBrowser, url: "about:preferences#privacy" },
    async function (browser) {
      let checkbox = browser.contentDocument.getElementById(GPC_CHECKBOX_ID);
      ok(!checkbox.checked, "checkbox is not checked in DISABLED mode");
    }
  );

  await SpecialPowers.popPrefEnv();
});

// Test that toggling the checkbox toggles the mode pref value as expected
add_task(async function test_checkbox_modifies_prefs() {
  await SpecialPowers.pushPrefEnv({
    set: [
      [FEATURE_PREF, true],
      [MODE_PREF, false],
    ],
  });

  await BrowserTestUtils.withNewTab(
    { gBrowser, url: "about:preferences#privacy" },
    async function (browser) {
      let gpc_checkbox =
        browser.contentDocument.getElementById(GPC_CHECKBOX_ID);
      let dnt_checkbox =
        browser.contentDocument.getElementById(DNT_CHECKBOX_ID);
      let section = browser.contentDocument.getElementById(SECTION_ID);

      section.scrollIntoView();

      Assert.ok(
        !gpc_checkbox.checked && !dnt_checkbox.checked,
        "initially, the checkbox should be unchecked"
      );

      await BrowserTestUtils.synthesizeMouseAtCenter(
        "#" + GPC_CHECKBOX_ID,
        {},
        browser
      );
      Assert.ok(
        gpc_checkbox.checked && !dnt_checkbox.checked,
        "gpc checkbox should be checked"
      );
      Assert.equal(
        true,
        Services.prefs.getBoolPref(MODE_PREF),
        "GPC should be on after checking the checkbox"
      );
      Assert.equal(
        false,
        Services.prefs.getBoolPref(DNT_PREF),
        "DNT should still be disabled after checking the checkbox"
      );

      await BrowserTestUtils.synthesizeMouseAtCenter(
        "#" + DNT_CHECKBOX_ID,
        {},
        browser
      );
      Assert.ok(
        gpc_checkbox.checked && dnt_checkbox.checked,
        "both checkboxes are enabled"
      );
      Assert.equal(
        true,
        Services.prefs.getBoolPref(MODE_PREF),
        "GPC should still be on after checking the DNT checkbox"
      );
      Assert.equal(
        true,
        Services.prefs.getBoolPref(DNT_PREF),
        "DNT should still be enabled after checking the checkbox"
      );

      await BrowserTestUtils.synthesizeMouseAtCenter(
        "#" + DNT_CHECKBOX_ID,
        {},
        browser
      );
      await BrowserTestUtils.synthesizeMouseAtCenter(
        "#" + GPC_CHECKBOX_ID,
        {},
        browser
      );
      Assert.ok(
        !gpc_checkbox.checked && !dnt_checkbox.checked,
        "both checkboxes are disabled"
      );
      Assert.equal(
        false,
        Services.prefs.getBoolPref(MODE_PREF),
        "GPC should be unchecked"
      );
      Assert.equal(
        false,
        Services.prefs.getBoolPref(DNT_PREF),
        "DNT should be unchecked"
      );
    }
  );

  await SpecialPowers.popPrefEnv();
});
