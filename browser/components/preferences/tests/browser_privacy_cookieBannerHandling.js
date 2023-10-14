/* Any copyright is dedicated to the Public Domain.
 * https://creativecommons.org/publicdomain/zero/1.0/ */

// This file tests the Privacy pane's Cookie Banner Handling UI.

"use strict";

const FEATURE_PREF = "cookiebanners.ui.desktop.enabled";
const MODE_PREF = "cookiebanners.service.mode";
const PBM_MODE_PREF = "cookiebanners.service.mode.privateBrowsing";

const GROUPBOX_ID = "cookieBannerHandlingGroup";
const CHECKBOX_ID = "handleCookieBanners";

// Test the section is hidden on page load if the feature pref is disabled.
add_task(async function test_section_hidden_when_feature_flag_disabled() {
  await SpecialPowers.pushPrefEnv({
    set: [
      [FEATURE_PREF, false],
      [PBM_MODE_PREF, Ci.nsICookieBannerService.MODE_DISABLED],
    ],
  });

  await BrowserTestUtils.withNewTab(
    { gBrowser, url: "about:preferences#privacy" },
    async function (browser) {
      let groupbox = browser.contentDocument.getElementById(GROUPBOX_ID);
      is_element_hidden(groupbox, "#cookieBannerHandlingGroup is hidden");
    }
  );

  await SpecialPowers.popPrefEnv();
});

// Test the section is shown on page load if the feature pref is enabled.
add_task(async function test_section_shown_when_feature_flag_enabled() {
  await SpecialPowers.pushPrefEnv({
    set: [
      [FEATURE_PREF, true],
      [PBM_MODE_PREF, Ci.nsICookieBannerService.MODE_DISABLED],
    ],
  });

  await BrowserTestUtils.withNewTab(
    { gBrowser, url: "about:preferences#privacy" },
    async function (browser) {
      let groupbox = browser.contentDocument.getElementById(GROUPBOX_ID);
      is_element_visible(groupbox, "#cookieBannerHandlingGroup is visible");
    }
  );

  await SpecialPowers.popPrefEnv();
});

// Test the checkbox is unchecked in DISABLED mode.
add_task(async function test_checkbox_unchecked_disabled_mode() {
  await SpecialPowers.pushPrefEnv({
    set: [
      [FEATURE_PREF, true],
      [PBM_MODE_PREF, Ci.nsICookieBannerService.MODE_DISABLED],
    ],
  });

  await BrowserTestUtils.withNewTab(
    { gBrowser, url: "about:preferences#privacy" },
    async function (browser) {
      let checkbox = browser.contentDocument.getElementById(CHECKBOX_ID);
      ok(!checkbox.checked, "checkbox is not checked in DISABLED mode");
    }
  );

  await SpecialPowers.popPrefEnv();
});

// Test the checkbox is checked in REJECT mode.
add_task(async function test_checkbox_checked_reject_mode() {
  await SpecialPowers.pushPrefEnv({
    set: [
      [FEATURE_PREF, true],
      [PBM_MODE_PREF, Ci.nsICookieBannerService.MODE_REJECT],
    ],
  });

  await BrowserTestUtils.withNewTab(
    { gBrowser, url: "about:preferences#privacy" },
    async function (browser) {
      let checkbox = browser.contentDocument.getElementById(CHECKBOX_ID);
      ok(checkbox.checked, "checkbox is checked in REJECT mode");
    }
  );

  await SpecialPowers.popPrefEnv();
});

// Test that toggling the checkbox toggles the mode pref value as expected
add_task(async function test_checkbox_modifies_prefs() {
  await SpecialPowers.pushPrefEnv({
    set: [
      [FEATURE_PREF, true],
      [MODE_PREF, Ci.nsICookieBannerService.MODE_DISABLED],
      [PBM_MODE_PREF, Ci.nsICookieBannerService.MODE_DISABLED],
    ],
  });

  await BrowserTestUtils.withNewTab(
    { gBrowser, url: "about:preferences#privacy" },
    async function (browser) {
      let checkboxSelector = "#" + CHECKBOX_ID;
      let checkbox = browser.contentDocument.querySelector(checkboxSelector);
      let section = browser.contentDocument.getElementById(GROUPBOX_ID);

      section.scrollIntoView();

      Assert.ok(
        !checkbox.checked,
        "initially, the checkbox should be unchecked"
      );

      await BrowserTestUtils.synthesizeMouseAtCenter(
        checkboxSelector,
        {},
        browser
      );
      Assert.ok(checkbox.checked, "checkbox should be checked");

      Assert.equal(
        Ci.nsICookieBannerService.MODE_REJECT,
        Services.prefs.getIntPref(PBM_MODE_PREF),
        "cookie banner handling mode for PBM should be set to REJECT after checking the checkbox"
      );

      Assert.equal(
        Ci.nsICookieBannerService.MODE_DISABLED,
        Services.prefs.getIntPref(MODE_PREF),
        "non-PBM cookie banner handling mode should be unchanged after checking the checkbox"
      );

      await BrowserTestUtils.synthesizeMouseAtCenter(
        checkboxSelector,
        {},
        browser
      );
      Assert.ok(!checkbox.checked, "checkbox should be unchecked");
      Assert.equal(
        Ci.nsICookieBannerService.MODE_DISABLED,
        Services.prefs.getIntPref(PBM_MODE_PREF),
        "cookie banner handling mode for PBM should be set to DISABLED after unchecking the checkbox"
      );

      // this ensures the off-ramp for non-PBM pref is working
      Assert.equal(
        Ci.nsICookieBannerService.MODE_DISABLED,
        Services.prefs.getIntPref(MODE_PREF),
        "non-PBM cookie banner handling mode should be set to DISABLED after unchecking the checkbox"
      );
    }
  );

  await SpecialPowers.popPrefEnv();
});
