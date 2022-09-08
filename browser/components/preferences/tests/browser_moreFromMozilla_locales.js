/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const { EnterprisePolicyTesting } = ChromeUtils.importESModule(
  "resource://testing-common/EnterprisePolicyTesting.sys.mjs"
);

let { Region } = ChromeUtils.import("resource://gre/modules/Region.jsm");

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
  let mobileCard = doc.getElementById("firefox-mobile");
  let rallyPromoCard = doc.getElementById("mozilla-rally");

  return {
    vpnPromoCard,
    mobileCard,
    rallyPromoCard,
  };
}

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

add_task(
  async function test_rally_promo_with_approved_home_region_and_language() {
    // Only show the Rally promo when US is the region and English is the langauge
    setupRegions("US");

    let { rallyPromoCard, mobileCard } = await getPromoCards();

    ok(rallyPromoCard, "The Rally promo is visible");
    ok(mobileCard, "The Mobile promo is visible");

    setupRegions(initialHomeRegion, initialCurrentRegion); // revert changes to regions
    BrowserTestUtils.removeTab(gBrowser.selectedTab);
  }
);

add_task(async function test_rally_promo_with_unapproved_home_region() {
  setupRegions("IS");

  let { rallyPromoCard, mobileCard } = await getPromoCards();

  ok(!rallyPromoCard, "The Rally promo is not visible");
  ok(mobileCard, "The Mobile promo is visible");

  setupRegions(initialHomeRegion, initialCurrentRegion); // revert changes to regions
  BrowserTestUtils.removeTab(gBrowser.selectedTab);
});

add_task(async function test_rally_promo_with_unapproved_current_region() {
  setupRegions("US", "IS");

  let { rallyPromoCard, mobileCard } = await getPromoCards();

  ok(!rallyPromoCard, "The Rally promo is not visible");
  ok(mobileCard, "The Mobile promo is visible");

  setupRegions(initialHomeRegion, initialCurrentRegion); // revert changes to regions
  BrowserTestUtils.removeTab(gBrowser.selectedTab);
});

add_task(async function test_rally_promo_with_unapproved_language() {
  // Rally promo should be hidden in the US for languages other than English
  setupRegions("US");
  const initialLanguage = Services.locale.appLocaleAsBCP47;
  setLocale("ko-KR");

  let { rallyPromoCard, mobileCard } = await getPromoCards();

  ok(!rallyPromoCard, "The Rally promo is not visible");
  ok(mobileCard, "The Mobile promo is visible");

  setupRegions(initialHomeRegion, initialCurrentRegion); // revert changes to regions
  // revert changes to language
  setLocale(initialLanguage);
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

  ok(!BrowserTestUtils.is_hidden(emailLink), "Email link should be visible");

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

    ok(BrowserTestUtils.is_hidden(emailLink), "Email link should be hidden");

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
