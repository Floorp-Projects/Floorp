/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const { EnterprisePolicyTesting } = ChromeUtils.importESModule(
  "resource://testing-common/EnterprisePolicyTesting.sys.mjs"
);

let { Region } = ChromeUtils.importESModule(
  "resource://gre/modules/Region.sys.mjs"
);

const initialHomeRegion = Region._home;
const initialCurrentRegion = Region._current;

// Helper to run tests for specific regions
async function setupRegions(home, current) {
  Region._setHomeRegion(home || "");
  Region._setCurrentRegion(current || "");
}

function setLocale(language) {
  Services.locale.availableLocales = [language];
  Services.locale.requestedLocales = [language];
}

async function clearPolicies() {
  // Ensure no active policies are set
  await EnterprisePolicyTesting.setupPolicyEngineWithJson("");
}

async function getPromoCards() {
  await openPreferencesViaOpenPreferencesAPI("paneMoreFromMozilla", {
    leaveOpen: true,
  });

  let doc = gBrowser.contentDocument;
  let vpnPromoCard = doc.getElementById("mozilla-vpn");
  let monitorPromoCard = doc.getElementById("mozilla-monitor");
  let mobileCard = doc.getElementById("firefox-mobile");
  let relayPromoCard = doc.getElementById("firefox-relay");

  return {
    vpnPromoCard,
    monitorPromoCard,
    mobileCard,
    relayPromoCard,
  };
}

let mockFxA, unmockFxA;

// The Relay promo is only shown if the default FxA instance is detected, and
// tests override it to a dummy address, so we need to make the dummy address
// appear like it's the default (using the actual default instance might cause a
// remote connection, crashing the test harness).
add_setup(async function () {
  let { mock, unmock } = await mockDefaultFxAInstance();
  mockFxA = mock;
  unmockFxA = unmock;
});

add_task(async function test_VPN_promo_enabled() {
  await clearPolicies();
  await SpecialPowers.pushPrefEnv({
    set: [
      ["browser.preferences.moreFromMozilla", true],
      ["browser.vpn_promo.enabled", true],
    ],
  });

  let { vpnPromoCard, mobileCard } = await getPromoCards();

  ok(vpnPromoCard, "The VPN promo is visible");
  ok(mobileCard, "The Mobile promo is visible");

  BrowserTestUtils.removeTab(gBrowser.selectedTab);
});

add_task(async function test_VPN_promo_disabled() {
  await clearPolicies();
  await SpecialPowers.pushPrefEnv({
    set: [["browser.vpn_promo.enabled", false]],
  });

  let { vpnPromoCard, mobileCard } = await getPromoCards();

  ok(!vpnPromoCard, "The VPN promo is not visible");
  ok(mobileCard, "The Mobile promo is visible");

  Services.prefs.clearUserPref("browser.vpn_promo.enabled");
  BrowserTestUtils.removeTab(gBrowser.selectedTab);
});

add_task(async function test_VPN_promo_in_disallowed_home_region() {
  await clearPolicies();
  const disallowedRegion = "SY";

  setupRegions(disallowedRegion);

  // Promo should not show in disallowed regions even when vpn_promo pref is enabled
  await SpecialPowers.pushPrefEnv({
    set: [["browser.vpn_promo.enabled", true]],
  });

  let { vpnPromoCard, mobileCard } = await getPromoCards();

  ok(!vpnPromoCard, "The VPN promo is not visible");
  ok(mobileCard, "The Mobile promo is visible");

  setupRegions(initialHomeRegion, initialCurrentRegion); // revert changes to regions
  BrowserTestUtils.removeTab(gBrowser.selectedTab);
});

add_task(async function test_VPN_promo_in_illegal_home_region() {
  await clearPolicies();
  const illegalRegion = "CN";

  setupRegions(illegalRegion);

  // Promo should not show in illegal regions even if the list of disallowed regions is somehow altered (though changing this preference is blocked)
  await SpecialPowers.pushPrefEnv({
    set: [["browser.vpn_promo.disallowedRegions", "SY, CU"]],
  });

  let { vpnPromoCard, mobileCard } = await getPromoCards();

  ok(!vpnPromoCard, "The VPN promo is not visible");
  ok(mobileCard, "The Mobile promo is visible");

  setupRegions(initialHomeRegion, initialCurrentRegion); // revert changes to regions
  BrowserTestUtils.removeTab(gBrowser.selectedTab);
});

add_task(async function test_VPN_promo_in_disallowed_current_region() {
  await clearPolicies();
  const allowedRegion = "US";
  const disallowedRegion = "SY";

  setupRegions(allowedRegion, disallowedRegion);

  // Promo should not show in disallowed regions even when vpn_promo pref is enabled
  await SpecialPowers.pushPrefEnv({
    set: [["browser.vpn_promo.enabled", true]],
  });

  let { vpnPromoCard, mobileCard } = await getPromoCards();

  ok(!vpnPromoCard, "The VPN promo is not visible");
  ok(mobileCard, "The Mobile promo is visible");

  setupRegions(initialHomeRegion, initialCurrentRegion); // revert changes to regions
  BrowserTestUtils.removeTab(gBrowser.selectedTab);
});

add_task(async function test_VPN_promo_in_illegal_current_region() {
  await clearPolicies();
  const allowedRegion = "US";
  const illegalRegion = "CN";

  setupRegions(allowedRegion, illegalRegion);

  // Promo should not show in illegal regions even if the list of disallowed regions is somehow altered (though changing this preference is blocked)
  await SpecialPowers.pushPrefEnv({
    set: [["browser.vpn_promo.disallowedRegions", "SY, CU"]],
  });

  let { vpnPromoCard, mobileCard } = await getPromoCards();

  ok(!vpnPromoCard, "The VPN promo is not visible");
  ok(mobileCard, "The Mobile promo is visible");

  setupRegions(initialHomeRegion, initialCurrentRegion); // revert changes to regions
  BrowserTestUtils.removeTab(gBrowser.selectedTab);
});

add_task(async function test_aboutpreferences_partnerCNRepack() {
  let defaultBranch = Services.prefs.getDefaultBranch(null);
  defaultBranch.setCharPref("distribution.id", "MozillaOnline");

  await SpecialPowers.pushPrefEnv({
    set: [
      ["browser.preferences.moreFromMozilla", true],
      ["browser.preferences.moreFromMozilla.template", "simple"],
    ],
  });
  await openPreferencesViaOpenPreferencesAPI("paneMoreFromMozilla", {
    leaveOpen: true,
  });

  let doc = gBrowser.contentDocument;
  let tab = gBrowser.selectedTab;

  let productCards = doc.querySelectorAll("vbox.simple");
  Assert.ok(productCards, "Simple template loaded");

  const expectedUrl = "https://www.firefox.com.cn/browsers/mobile/";

  let link = doc.getElementById("simple-fxMobile");
  Assert.ok(link.getAttribute("href").startsWith(expectedUrl));

  defaultBranch.setCharPref("distribution.id", "");
  BrowserTestUtils.removeTab(tab);
});

add_task(async function test_send_to_device_email_link_for_supported_locale() {
  // Email is supported for Brazilian Portuguese
  const supportedLocale = "pt-BR";
  const initialLocale = Services.locale.appLocaleAsBCP47;
  setLocale(supportedLocale);

  await SpecialPowers.pushPrefEnv({
    set: [["browser.preferences.moreFromMozilla.template", "simple"]],
  });

  await openPreferencesViaOpenPreferencesAPI("paneMoreFromMozilla", {
    leaveOpen: true,
  });

  let doc = gBrowser.contentDocument;
  let emailLink = doc.getElementById("simple-qr-code-send-email");

  ok(!BrowserTestUtils.isHidden(emailLink), "Email link should be visible");

  await SpecialPowers.popPrefEnv();
  BrowserTestUtils.removeTab(gBrowser.selectedTab);
  setLocale(initialLocale); // revert changes to language
});

add_task(
  async function test_send_to_device_email_link_for_unsupported_locale() {
    // Email is not supported for Afrikaans
    const unsupportedLocale = "af";
    const initialLocale = Services.locale.appLocaleAsBCP47;
    setLocale(unsupportedLocale);

    await SpecialPowers.pushPrefEnv({
      set: [["browser.preferences.moreFromMozilla.template", "simple"]],
    });

    await openPreferencesViaOpenPreferencesAPI("paneMoreFromMozilla", {
      leaveOpen: true,
    });

    let doc = gBrowser.contentDocument;
    let emailLink = doc.getElementById("simple-qr-code-send-email");

    ok(BrowserTestUtils.isHidden(emailLink), "Email link should be hidden");

    await SpecialPowers.popPrefEnv();
    BrowserTestUtils.removeTab(gBrowser.selectedTab);
    setLocale(initialLocale); // revert changes to language
  }
);

add_task(
  async function test_VPN_promo_in_unsupported_current_region_with_supported_home_region() {
    await clearPolicies();
    const supportedRegion = "US";
    const unsupportedRegion = "LY";

    setupRegions(supportedRegion, unsupportedRegion);

    let { vpnPromoCard, mobileCard } = await getPromoCards();

    ok(vpnPromoCard, "The VPN promo is visible");
    ok(mobileCard, "The Mobile promo is visible");

    setupRegions(initialHomeRegion, initialCurrentRegion); // revert changes to regions
    BrowserTestUtils.removeTab(gBrowser.selectedTab);
  }
);

add_task(
  async function test_VPN_promo_in_supported_current_region_with_unsupported_home_region() {
    await clearPolicies();
    const supportedRegion = "US";
    const unsupportedRegion = "LY";

    setupRegions(unsupportedRegion, supportedRegion);

    let { vpnPromoCard, mobileCard } = await getPromoCards();

    ok(vpnPromoCard, "The VPN promo is visible");
    ok(mobileCard, "The Mobile promo is visible");

    setupRegions(initialHomeRegion, initialCurrentRegion); // revert changes to regions
    BrowserTestUtils.removeTab(gBrowser.selectedTab);
  }
);

add_task(async function test_VPN_promo_with_active_enterprise_policy() {
  // set up an arbitrary enterprise policy
  await EnterprisePolicyTesting.setupPolicyEngineWithJson({
    policies: {
      EnableTrackingProtection: {
        Value: true,
      },
    },
  });

  let { vpnPromoCard, mobileCard } = await getPromoCards();
  ok(!vpnPromoCard, "The VPN promo is not visible");
  ok(mobileCard, "The Mobile promo is visible");

  setupRegions(initialHomeRegion, initialCurrentRegion); // revert changes to regions
  await clearPolicies();
  BrowserTestUtils.removeTab(gBrowser.selectedTab);
});

add_task(async function test_relay_promo_with_supported_fxa_server() {
  await clearPolicies();

  let { relayPromoCard } = await getPromoCards();
  ok(relayPromoCard, "The Relay promo is visible");

  BrowserTestUtils.removeTab(gBrowser.selectedTab);
});

add_task(async function test_relay_promo_with_unsupported_fxa_server() {
  await clearPolicies();
  // Set the default pref value to something other than the current value so it
  // will appear to be user-set and treated as invalid (actually setting the
  // pref would cause a remote connection and crash the test harness)
  unmockFxA();

  let { relayPromoCard } = await getPromoCards();
  ok(!relayPromoCard, "The Relay promo is not visible");

  BrowserTestUtils.removeTab(gBrowser.selectedTab);
  mockFxA();
});

add_task(async function test_Monitor_US_region_desc() {
  const supportedRegion = "US";
  setupRegions(supportedRegion);

  let { monitorPromoCard } = await getPromoCards();
  ok(monitorPromoCard, "The Monitor promo is visible");

  let monitorDescElement =
    monitorPromoCard.nextElementSibling.querySelector(".description");
  is(
    monitorDescElement.getAttribute("data-l10n-id"),
    "more-from-moz-mozilla-monitor-us-description",
    "US Region desc set"
  );

  setupRegions(initialHomeRegion, initialCurrentRegion); // revert changes to regions
  BrowserTestUtils.removeTab(gBrowser.selectedTab);
});

add_task(async function test_Monitor_global_region_desc() {
  const supportedRegion = "UK";
  setupRegions(supportedRegion);

  let { monitorPromoCard } = await getPromoCards();
  ok(monitorPromoCard, "The Monitor promo is visible");

  let monitorDescElement =
    monitorPromoCard.nextElementSibling.querySelector(".description");
  is(
    monitorDescElement.getAttribute("data-l10n-id"),
    "more-from-moz-mozilla-monitor-global-description",
    "Global Region desc set"
  );

  setupRegions(initialHomeRegion, initialCurrentRegion); // revert changes to regions
  BrowserTestUtils.removeTab(gBrowser.selectedTab);
});
