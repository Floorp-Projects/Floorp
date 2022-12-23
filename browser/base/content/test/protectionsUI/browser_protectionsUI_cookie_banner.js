/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * Tests the cookie banner handling section in the protections panel.
 */

const { TelemetryTestUtils } = ChromeUtils.importESModule(
  "resource://testing-common/TelemetryTestUtils.sys.mjs"
);

const {
  MODE_DISABLED,
  MODE_REJECT,
  MODE_REJECT_OR_ACCEPT,
  MODE_DETECT_ONLY,
  MODE_UNSET,
} = Ci.nsICookieBannerService;

/**
 * Determines whether the cookie banner section in the protections panel should
 * be visible with the given configuration.
 * @param {*} options - Configuration to test.
 * @param {Number} options.featureMode - nsICookieBannerService::Modes value for
 * normal browsing.
 * @param {Number} options.featureModePBM - nsICookieBannerService::Modes value
 * for private browsing.
 * @param {boolean} options.visibilityPref - State of the cookie banner UI
 * visibility pref.
 * @param {boolean} options.testPBM - Whether the window is in private browsing
 * mode (true) or not (false).
 * @returns {boolean} Whether the section should be visible for the given
 * config.
 */
function cookieBannerSectionIsVisible({
  featureMode,
  featureModePBM,
  visibilityPref,
  testPBM,
}) {
  if (!visibilityPref) {
    return false;
  }

  return (
    (testPBM &&
      featureModePBM != MODE_DISABLED &&
      featureModePBM != MODE_DETECT_ONLY) ||
    (!testPBM &&
      featureMode != MODE_DISABLED &&
      featureMode != MODE_DETECT_ONLY)
  );
}

/**
 * Runs a visibility test of the cookie banner section in the protections panel.
 * @param {*} options - Test options.
 * @param {Window} options.win - Browser window to use for testing. It's
 * browsing mode should match the testPBM variable.
 * @param {Number} options.featureMode - nsICookieBannerService::Modes value for
 * normal browsing.
 * @param {Number} options.featureModePBM - nsICookieBannerService::Modes value
 * for private browsing.
 * @param {boolean} options.visibilityPref - State of the cookie banner UI
 * visibility pref.
 * @param {boolean} options.testPBM - Whether the window is in private browsing
 * mode (true) or not (false).
 * @returns {Promise} Resolves once the test is complete.
 */
async function testSectionVisibility({
  win,
  featureMode,
  featureModePBM,
  visibilityPref,
  testPBM,
}) {
  info(
    "testSectionVisibility " +
      JSON.stringify({ featureMode, featureModePBM, visibilityPref, testPBM })
  );
  // initialize the pref environment
  await SpecialPowers.pushPrefEnv({
    set: [
      ["cookiebanners.service.mode", featureMode],
      ["cookiebanners.service.mode.privateBrowsing", featureModePBM],
      ["cookiebanners.ui.desktop.enabled", visibilityPref],
    ],
  });

  // Open a tab with example.com so the protections panel can be opened.
  await BrowserTestUtils.withNewTab(
    { gBrowser: win.gBrowser, url: "https://example.com" },
    async () => {
      await openProtectionsPanel(null, win);

      // Get panel elements to test
      let el = {
        section: win.document.getElementById(
          "protections-popup-cookie-banner-section"
        ),
        sectionSeparator: win.document.getElementById(
          "protections-popup-cookie-banner-section-separator"
        ),
        switch: win.document.getElementById(
          "protections-popup-cookie-banner-switch"
        ),
      };

      let expectVisible = cookieBannerSectionIsVisible({
        featureMode,
        featureModePBM,
        visibilityPref,
        testPBM,
      });
      is(
        BrowserTestUtils.is_visible(el.section),
        expectVisible,
        `Cookie banner section should be ${
          expectVisible ? "visible" : "not visible"
        }.`
      );
      is(
        BrowserTestUtils.is_visible(el.sectionSeparator),
        expectVisible,
        `Cookie banner section separator should be ${
          expectVisible ? "visible" : "not visible"
        }.`
      );
      is(
        BrowserTestUtils.is_visible(el.switch),
        expectVisible,
        `Cookie banner switch should be ${
          expectVisible ? "visible" : "not visible"
        }.`
      );
    }
  );

  await SpecialPowers.popPrefEnv();
}

/**
 * Tests cookie banner section visibility state in different configurations.
 */
add_task(async function test_section_visibility() {
  // Test all combinations of cookie banner service modes and normal and
  // private browsing.
  for (let testPBM of [false, true]) {
    let win = window;
    // Open a new private window to test the panel in for testing PBM, otherwise
    // reuse the existing window.
    if (testPBM) {
      win = await BrowserTestUtils.openNewBrowserWindow({
        private: true,
      });
      win.focus();
    }

    for (let featureMode of [
      MODE_DISABLED,
      MODE_REJECT,
      MODE_REJECT_OR_ACCEPT,
      MODE_DETECT_ONLY,
    ]) {
      for (let featureModePBM of [
        MODE_DISABLED,
        MODE_REJECT,
        MODE_REJECT_OR_ACCEPT,
        MODE_DETECT_ONLY,
      ]) {
        await testSectionVisibility({
          win,
          featureMode,
          featureModePBM,
          testPBM,
          visibilityPref: true,
        });
      }
    }

    if (testPBM) {
      await BrowserTestUtils.closeWindow(win);
    }
  }
});

/**
 * Tests that the cookie banner section is only visible if enabled by UI pref.
 */
add_task(async function test_section_visibility_pref() {
  for (let visibilityPref of [false, true]) {
    await testSectionVisibility({
      win: window,
      featureMode: MODE_REJECT,
      featureModePBM: MODE_DISABLED,
      testPBM: false,
      visibilityPref,
    });
  }
});

/**
 * Test the state of the per-site exception switch in the cookie banner section
 * and whether a matching per-site exception is set.
 * @param {*} options
 * @param {Window} options.win - Chrome window to test exception for (selected
 * tab).
 * @param {boolean} options.isPBM - Whether the given window is in private
 * browsing mode.
 * @param {boolean} options.expectEnabled - Whether the switch is expected to be
 * enabled, this also indicates no exception set.
 */
function assertSwitchAndPrefState({ win, isPBM, expectEnabled }) {
  let el = {
    switch: win.document.getElementById(
      "protections-popup-cookie-banner-switch"
    ),
    labelON: win.document.querySelector(
      ".protections-popup-cookie-banner-switch-on-header"
    ),
    labelOFF: win.document.querySelector(
      ".protections-popup-cookie-banner-switch-off-header"
    ),
  };

  info("Test switch state.");
  ok(BrowserTestUtils.is_visible(el.switch), "Switch should be visible");
  is(
    el.switch.hasAttribute("enabled"),
    expectEnabled,
    `Switch is ${expectEnabled ? "enabled" : "disabled"}.`
  );

  info("Test switch labels.");
  if (expectEnabled) {
    ok(BrowserTestUtils.is_visible(el.labelON), "ON label should be visible");
    ok(
      !BrowserTestUtils.is_visible(el.labelOFF),
      "OFF label should not be visible"
    );
  } else {
    ok(
      !BrowserTestUtils.is_visible(el.labelON),
      "ON label should not be visible"
    );
    ok(BrowserTestUtils.is_visible(el.labelOFF), "OFF label should be visible");
  }

  info("Test per-site exception state.");
  let currentURI = win.gBrowser.currentURI;
  let pref = Services.cookieBanners.getDomainPref(currentURI, isPBM);

  if (expectEnabled) {
    is(
      pref,
      MODE_UNSET,
      `There should be no per-site exception for ${currentURI.spec}.`
    );
  } else {
    is(
      pref,
      MODE_DISABLED,
      `There should be a per-site exception for ${currentURI.spec}.`
    );
  }
}

/**
 * Test the telemetry associated with the cookie banner toggle. To be called
 * after interacting with the toggle.
 * @param {*} options
 * @param {boolean|null} - Expected telemetry state matching the button state.
 * button on = true = cookieb_toggle_on event. Pass null to expect no event
 * recorded.
 */
function assertTelemetryState({ expectEnabled = null } = {}) {
  info("Test telemetry state.");

  let events = [];
  const CATEGORY = "security.ui.protectionspopup";
  const METHOD = "click";

  if (expectEnabled != null) {
    events.push({
      category: CATEGORY,
      method: METHOD,
      object: expectEnabled ? "cookieb_toggle_on" : "cookieb_toggle_off",
    });
  }

  // Assert event state and clear event list.
  TelemetryTestUtils.assertEvents(events, {
    category: CATEGORY,
    method: METHOD,
  });
}

/**
 * Tests the cookie banner section per-site preference toggle.
 */
add_task(async function test_section_toggle() {
  // initialize the pref environment
  await SpecialPowers.pushPrefEnv({
    set: [
      ["cookiebanners.service.mode", MODE_REJECT_OR_ACCEPT],
      ["cookiebanners.service.mode.privateBrowsing", MODE_REJECT],
      ["cookiebanners.ui.desktop.enabled", true],
    ],
  });

  // Test both normal and private browsing windows. For normal windows we reuse
  // the existing one, for private windows we need to open a new window.
  for (let testPBM of [false, true]) {
    let win = window;
    if (testPBM) {
      win = await BrowserTestUtils.openNewBrowserWindow({
        private: true,
      });
    }
    await BrowserTestUtils.withNewTab(
      { gBrowser: win.gBrowser, url: "https://example.com" },
      async () => {
        await openProtectionsPanel(null, win);

        let switchEl = win.document.getElementById(
          "protections-popup-cookie-banner-switch"
        );

        info("Testing initial switch ON state.");
        assertSwitchAndPrefState({
          win,
          isPBM: testPBM,
          switchEl,
          expectEnabled: true,
        });
        assertTelemetryState();

        info("Testing switch state after toggle OFF");
        switchEl.click();
        assertSwitchAndPrefState({
          win,
          isPBM: testPBM,
          switchEl,
          expectEnabled: false,
        });
        assertTelemetryState({ expectEnabled: false });

        info("Reopen the panel to test the initial switch OFF state.");
        await closeProtectionsPanel(win);
        await openProtectionsPanel(null, win);
        assertSwitchAndPrefState({
          win,
          isPBM: testPBM,
          switchEl,
          expectEnabled: false,
        });
        assertTelemetryState();

        info("Testing switch state after toggle ON.");
        switchEl.click();
        assertSwitchAndPrefState({
          win,
          isPBM: testPBM,
          switchEl,
          expectEnabled: true,
        });
        assertTelemetryState({ expectEnabled: true });

        info("Reopen the panel to test the initial switch ON state.");
        await closeProtectionsPanel(win);
        await openProtectionsPanel(null, win);
        assertSwitchAndPrefState({
          win,
          isPBM: testPBM,
          switchEl,
          expectEnabled: true,
        });
        assertTelemetryState();
      }
    );

    if (testPBM) {
      await BrowserTestUtils.closeWindow(win);
    }
  }
});
