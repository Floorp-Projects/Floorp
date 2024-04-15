/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

/*
 * Test to verify that the SERP impression event correctly records whether the
 * user is logged in to a provider's account at the time of SERP load.
 */

"use strict";

const TEST_PROVIDER_INFO = [
  {
    telemetryId: "example",
    searchPageRegexp:
      /^https:\/\/example.org\/browser\/browser\/components\/search\/test\/browser\/telemetry\/searchTelemetry(?:Ad)?/,
    queryParamNames: ["s"],
    accountCookies: [
      {
        host: "accounts.google.com",
        name: "SID",
      },
    ],
    codeParamName: "abc",
    taggedCodes: ["ff"],
    followOnParamNames: ["a"],
    extraAdServersRegexps: [/^https:\/\/example\.com\/ad2?/],
    components: [
      {
        type: SearchSERPTelemetryUtils.COMPONENTS.AD_LINK,
        default: true,
      },
    ],
  },
];

function simulateGoogleAccountSignIn() {
  // Manually set the cookie that is present when the client is signed in to a
  // Google account.
  Services.cookies.add(
    "accounts.google.com",
    "",
    "SID",
    "dummy_cookie_value",
    false,
    false,
    false,
    Date.now() + 1000 * 60 * 60,
    {},
    Ci.nsICookie.SAMESITE_NONE,
    Ci.nsICookie.SCHEME_HTTPS
  );
}

add_setup(async function () {
  SearchSERPTelemetry.overrideSearchTelemetryForTests(TEST_PROVIDER_INFO);
  await waitForIdle();
  // Enable local telemetry recording for the duration of the tests.
  let oldCanRecord = Services.telemetry.canRecordExtended;
  Services.telemetry.canRecordExtended = true;

  registerCleanupFunction(async () => {
    SearchSERPTelemetry.overrideSearchTelemetryForTests();
    Services.telemetry.canRecordExtended = oldCanRecord;
    resetTelemetry();
  });
});

add_task(async function test_not_signed_in_to_google_account() {
  info("Loading SERP while not signed in to Google account.");
  let url = getSERPUrl("searchTelemetry.html");
  let tab = await BrowserTestUtils.openNewForegroundTab(gBrowser, url);

  assertSERPTelemetry([
    {
      impression: {
        provider: "example",
        tagged: "true",
        partner_code: "ff",
        source: "unknown",
        is_shopping_page: "false",
        is_private: "false",
        shopping_tab_displayed: "false",
        is_signed_in: "false",
      },
    },
  ]);

  BrowserTestUtils.removeTab(tab);
  resetTelemetry();
});

add_task(async function test_signed_in_to_google_account() {
  simulateGoogleAccountSignIn();

  info("Loading SERP while signed in to Google account.");
  let url = getSERPUrl("searchTelemetry.html");
  let tab = await BrowserTestUtils.openNewForegroundTab(gBrowser, url);

  assertSERPTelemetry([
    {
      impression: {
        provider: "example",
        tagged: "true",
        partner_code: "ff",
        source: "unknown",
        is_shopping_page: "false",
        is_private: "false",
        shopping_tab_displayed: "false",
        is_signed_in: "true",
      },
    },
  ]);

  BrowserTestUtils.removeTab(tab);
  Services.cookies.removeAll();
  resetTelemetry();
});

add_task(async function test_toggle_google_account_signed_in_status() {
  info("Loading SERP while not signed in to Google account.");
  let url = getSERPUrl("searchTelemetry.html");
  let tab1 = await BrowserTestUtils.openNewForegroundTab(gBrowser, url);

  assertSERPTelemetry([
    {
      impression: {
        provider: "example",
        tagged: "true",
        partner_code: "ff",
        source: "unknown",
        is_shopping_page: "false",
        is_private: "false",
        shopping_tab_displayed: "false",
        is_signed_in: "false",
      },
    },
  ]);

  resetTelemetry();

  info("Signing in to Google account.");
  simulateGoogleAccountSignIn();

  info("Loading SERP after signing in to Google account.");
  let tab2 = await BrowserTestUtils.openNewForegroundTab(gBrowser, url);
  assertSERPTelemetry([
    {
      impression: {
        provider: "example",
        tagged: "true",
        partner_code: "ff",
        source: "unknown",
        is_shopping_page: "false",
        is_private: "false",
        shopping_tab_displayed: "false",
        is_signed_in: "true",
      },
    },
  ]);

  resetTelemetry();

  info("Signing out of Google account.");
  Services.cookies.removeAll();

  info("Loading SERP after signing out of Google account.");
  let tab3 = await BrowserTestUtils.openNewForegroundTab(gBrowser, url);
  assertSERPTelemetry([
    {
      impression: {
        provider: "example",
        tagged: "true",
        partner_code: "ff",
        source: "unknown",
        is_shopping_page: "false",
        is_private: "false",
        shopping_tab_displayed: "false",
        is_signed_in: "false",
      },
    },
  ]);

  BrowserTestUtils.removeTab(tab1);
  BrowserTestUtils.removeTab(tab2);
  BrowserTestUtils.removeTab(tab3);
  resetTelemetry();
});
