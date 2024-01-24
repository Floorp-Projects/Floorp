/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * Tests the cookie banner handling section in the protections panel.
 */

const { TelemetryTestUtils } = ChromeUtils.importESModule(
  "resource://testing-common/TelemetryTestUtils.sys.mjs"
);
const { sinon } = ChromeUtils.importESModule(
  "resource://testing-common/Sinon.sys.mjs"
);

const { MODE_DISABLED, MODE_REJECT, MODE_REJECT_OR_ACCEPT, MODE_UNSET } =
  Ci.nsICookieBannerService;

const exampleRules = JSON.stringify([
  {
    id: "4b18afb0-76db-4f9e-a818-ed9a783fae6a",
    cookies: {},
    click: {
      optIn: "#foo",
      presence: "#bar",
    },
    domains: ["example.com"],
  },
]);

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
  detectOnly,
  visibilityPref,
  testPBM,
}) {
  if (!visibilityPref) {
    return false;
  }
  if (detectOnly) {
    return false;
  }

  return (
    (testPBM && featureModePBM != MODE_DISABLED) ||
    (!testPBM && featureMode != MODE_DISABLED)
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
        BrowserTestUtils.isVisible(el.section),
        expectVisible,
        `Cookie banner section should be ${
          expectVisible ? "visible" : "not visible"
        }.`
      );
      is(
        BrowserTestUtils.isVisible(el.sectionSeparator),
        expectVisible,
        `Cookie banner section separator should be ${
          expectVisible ? "visible" : "not visible"
        }.`
      );
      is(
        BrowserTestUtils.isVisible(el.switch),
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
    ]) {
      for (let featureModePBM of [
        MODE_DISABLED,
        MODE_REJECT,
        MODE_REJECT_OR_ACCEPT,
      ]) {
        for (let detectOnly of [false, true]) {
          // Testing detect only mode for normal browsing is sufficient.
          if (detectOnly && featureModePBM != MODE_DISABLED) {
            continue;
          }
          await testSectionVisibility({
            win,
            featureMode,
            featureModePBM,
            detectOnly,
            testPBM,
            visibilityPref: true,
          });
        }
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
 * @param {string} options.expectedSwitchState - Whether the switch is expected to be
 * "on" (CBH enabled), "off" (user added exception), or "unsupported" (no rules for site).
 */
function assertSwitchAndPrefState({ win, isPBM, expectedSwitchState }) {
  let el = {
    section: win.document.getElementById(
      "protections-popup-cookie-banner-section"
    ),
    switch: win.document.getElementById(
      "protections-popup-cookie-banner-switch"
    ),
    labelON: win.document.querySelector(
      "#protections-popup-cookie-banner-detected"
    ),
    labelOFF: win.document.querySelector(
      "#protections-popup-cookie-banner-site-disabled"
    ),
    labelUNDETECTED: win.document.querySelector(
      "#protections-popup-cookie-banner-undetected"
    ),
  };

  let currentURI = win.gBrowser.currentURI;
  let pref = Services.cookieBanners.getDomainPref(currentURI, isPBM);
  if (expectedSwitchState == "on") {
    Assert.equal(
      el.section.dataset.state,
      "detected",
      "CBH switch is set to ON"
    );

    ok(BrowserTestUtils.isVisible(el.labelON), "ON label should be visible");
    ok(
      !BrowserTestUtils.isVisible(el.labelOFF),
      "OFF label should not be visible"
    );
    ok(
      !BrowserTestUtils.isVisible(el.labelUNDETECTED),
      "UNDETECTED label should not be visible"
    );

    is(
      pref,
      MODE_UNSET,
      `There should be no per-site exception for ${currentURI.spec}.`
    );
  } else if (expectedSwitchState === "off") {
    Assert.equal(
      el.section.dataset.state,
      "site-disabled",
      "CBH switch is set to OFF"
    );

    ok(
      !BrowserTestUtils.isVisible(el.labelON),
      "ON label should not be visible"
    );
    ok(BrowserTestUtils.isVisible(el.labelOFF), "OFF label should be visible");
    ok(
      !BrowserTestUtils.isVisible(el.labelUNDETECTED),
      "UNDETECTED label should not be visible"
    );

    is(
      pref,
      MODE_DISABLED,
      `There should be a per-site exception for ${currentURI.spec}.`
    );
  } else {
    Assert.equal(
      el.section.dataset.state,
      "undetected",
      "CBH not supported for site"
    );

    ok(
      !BrowserTestUtils.isVisible(el.labelON),
      "ON label should not be visible"
    );
    ok(
      !BrowserTestUtils.isVisible(el.labelOFF),
      "OFF label should not be visible"
    );
    ok(
      BrowserTestUtils.isVisible(el.labelUNDETECTED),
      "UNDETECTED label should be visible"
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
 * Test the cookie banner enable / disable by clicking the switch, then
 * clicking the on/off button in the cookie banner subview. Assumes the
 * protections panel is already open.
 *
 * @param {boolean} enable - Whether we want to enable or disable.
 * @param {Window} win - Current chrome window under test.
 */
async function toggleCookieBannerHandling(enable, win) {
  let switchEl = win.document.getElementById(
    "protections-popup-cookie-banner-switch"
  );
  let enableButton = win.document.getElementById(
    "protections-popup-cookieBannerView-enable-button"
  );
  let disableButton = win.document.getElementById(
    "protections-popup-cookieBannerView-disable-button"
  );
  let subView = win.document.getElementById(
    "protections-popup-cookieBannerView"
  );

  let subViewShownPromise = BrowserTestUtils.waitForEvent(subView, "ViewShown");
  switchEl.click();
  await subViewShownPromise;

  if (enable) {
    ok(BrowserTestUtils.isVisible(enableButton), "Enable button is visible");
    enableButton.click();
  } else {
    ok(BrowserTestUtils.isVisible(disableButton), "Disable button is visible");
    disableButton.click();
  }
}

function waitForProtectionsPopupHide(win = window) {
  return BrowserTestUtils.waitForEvent(
    win.document.getElementById("protections-popup"),
    "popuphidden"
  );
}

/**
 * Tests the cookie banner section per-site preference toggle.
 */
add_task(async function test_section_toggle() {
  requestLongerTimeout(3);

  // initialize the pref environment
  await SpecialPowers.pushPrefEnv({
    set: [
      ["cookiebanners.service.mode", MODE_REJECT_OR_ACCEPT],
      ["cookiebanners.service.mode.privateBrowsing", MODE_REJECT_OR_ACCEPT],
      ["cookiebanners.ui.desktop.enabled", true],
      ["cookiebanners.listService.testRules", exampleRules],
      ["cookiebanners.listService.testSkipRemoteSettings", true],
    ],
  });

  Services.cookieBanners.resetRules(false);
  await BrowserTestUtils.waitForCondition(
    () => !!Services.cookieBanners.rules.length,
    "waiting for Services.cookieBanners.rules.length to be greater than 0"
  );

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
        let clearSiteDataSpy = sinon.spy(window.SiteDataManager, "remove");

        await openProtectionsPanel(null, win);
        let switchEl = win.document.getElementById(
          "protections-popup-cookie-banner-switch"
        );
        info("Testing initial switch ON state.");
        assertSwitchAndPrefState({
          win,
          isPBM: testPBM,
          switchEl,
          expectedSwitchState: "on",
        });
        assertTelemetryState();

        info("Testing switch state after toggle OFF");
        let closePromise = waitForProtectionsPopupHide(win);
        await toggleCookieBannerHandling(false, win);
        await closePromise;
        if (testPBM) {
          Assert.ok(
            clearSiteDataSpy.notCalled,
            "clearSiteData should not be called in private browsing mode"
          );
        } else {
          Assert.ok(
            clearSiteDataSpy.calledOnce,
            "clearSiteData should be called in regular browsing mode"
          );
        }
        clearSiteDataSpy.restore();

        await openProtectionsPanel(null, win);
        assertSwitchAndPrefState({
          win,
          isPBM: testPBM,
          switchEl,
          expectedSwitchState: "off",
        });
        assertTelemetryState({ expectEnabled: false });

        info("Testing switch state after toggle ON.");
        closePromise = waitForProtectionsPopupHide(win);
        await toggleCookieBannerHandling(true, win);
        await closePromise;

        await openProtectionsPanel(null, win);
        assertSwitchAndPrefState({
          win,
          isPBM: testPBM,
          switchEl,
          expectedSwitchState: "on",
        });
        assertTelemetryState({ expectEnabled: true });
      }
    );

    if (testPBM) {
      await BrowserTestUtils.closeWindow(win);
    }
  }

  await SpecialPowers.popPrefEnv();
});
