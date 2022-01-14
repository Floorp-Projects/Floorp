/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const { ContentTaskUtils } = ChromeUtils.import(
  "resource://testing-common/ContentTaskUtils.jsm"
);

let { TelemetryTestUtils } = ChromeUtils.import(
  "resource://testing-common/TelemetryTestUtils.jsm"
);

let { Region } = ChromeUtils.import("resource://gre/modules/Region.jsm");

const initialHomeRegion = Region._home;
const intialCurrentRegion = Region._current;

// Helper to run tests for specific regions
async function setupRegions(home, current) {
  Region._setHomeRegion(home || "");
  Region._setCurrentRegion(current || "");
}

/**
 * Test that we don't show moreFromMozilla pane when it's disabled.
 */
add_task(async function testwhenPrefDisabled() {
  await SpecialPowers.pushPrefEnv({
    set: [["browser.preferences.moreFromMozilla", false]],
  });

  await openPreferencesViaOpenPreferencesAPI("paneGeneral", {
    leaveOpen: true,
  });
  let doc = gBrowser.contentDocument;

  let moreFromMozillaCategory = doc.getElementById(
    "category-more-from-mozilla"
  );
  ok(moreFromMozillaCategory, "The category exists");
  ok(moreFromMozillaCategory.hidden, "The category is hidden");

  BrowserTestUtils.removeTab(gBrowser.selectedTab);
});

add_task(async function testwhenPrefEnabledWithoutTemplatePref() {
  await SpecialPowers.pushPrefEnv({
    set: [["browser.preferences.moreFromMozilla", true]],
  });

  await openPreferencesViaOpenPreferencesAPI("paneGeneral", {
    leaveOpen: true,
  });
  let doc = gBrowser.contentDocument;

  let moreFromMozillaCategory = doc.getElementById(
    "category-more-from-mozilla"
  );
  ok(moreFromMozillaCategory, "The category exists");
  ok(!moreFromMozillaCategory.hidden, "The category is not hidden");

  let productCard = doc.querySelector(".mozilla-product-item");
  ok(productCard, "productCard found");

  BrowserTestUtils.removeTab(gBrowser.selectedTab);
});

add_task(async function test_aboutpreferences_event_telemetry() {
  Services.telemetry.clearEvents();
  Services.telemetry.setEventRecordingEnabled("aboutpreferences", true);

  await SpecialPowers.pushPrefEnv({
    set: [["browser.preferences.moreFromMozilla", true]],
  });
  await openPreferencesViaOpenPreferencesAPI("paneGeneral", {
    leaveOpen: true,
  });

  let doc = gBrowser.contentDocument;
  let moreFromMozillaCategory = doc.getElementById(
    "category-more-from-mozilla"
  );

  let clickedPromise = ContentTaskUtils.waitForEvent(
    moreFromMozillaCategory,
    "click"
  );
  moreFromMozillaCategory.click();
  await clickedPromise;

  TelemetryTestUtils.assertEvents(
    [["aboutpreferences", "show", "initial", "paneGeneral"]],
    { category: "aboutpreferences", method: "show", object: "initial" },
    { clear: false }
  );
  TelemetryTestUtils.assertEvents(
    [["aboutpreferences", "show", "click", "paneMoreFromMozilla"]],
    { category: "aboutpreferences", method: "show", object: "click" },
    { clear: false }
  );
  TelemetryTestUtils.assertNumberOfEvents(2);
  BrowserTestUtils.removeTab(gBrowser.selectedTab);
});

add_task(async function test_aboutpreferences_simple_template() {
  await SpecialPowers.pushPrefEnv({
    set: [
      ["browser.preferences.moreFromMozilla", true],
      ["browser.preferences.moreFromMozilla.template", "simple"],
    ],
  });
  await openPreferencesViaOpenPreferencesAPI("paneGeneral", {
    leaveOpen: true,
  });

  let doc = gBrowser.contentDocument;
  let moreFromMozillaCategory = doc.getElementById(
    "category-more-from-mozilla"
  );

  let clickedPromise = ContentTaskUtils.waitForEvent(
    moreFromMozillaCategory,
    "click"
  );
  moreFromMozillaCategory.click();
  await clickedPromise;

  let productCards = doc.querySelectorAll("div.simple");
  Assert.ok(productCards, "The product cards from simple template found");
  Assert.equal(productCards.length, 3, "3 product cards displayed");

  let qrCodeButtons = doc.querySelectorAll('.qr-code-box[hidden="false"]');
  Assert.equal(qrCodeButtons.length, 1, "1 qr-code box displayed");

  BrowserTestUtils.removeTab(gBrowser.selectedTab);
});

add_task(async function test_aboutpreferences_advanced_template() {
  await SpecialPowers.pushPrefEnv({
    set: [
      ["browser.preferences.moreFromMozilla", true],
      ["browser.preferences.moreFromMozilla.template", "advanced"],
      ["browser.vpn_promo.enabled", true],
    ],
  });
  await openPreferencesViaOpenPreferencesAPI("paneGeneral", {
    leaveOpen: true,
  });

  let doc = gBrowser.contentDocument;
  let moreFromMozillaCategory = doc.getElementById(
    "category-more-from-mozilla"
  );

  let clickedPromise = ContentTaskUtils.waitForEvent(
    moreFromMozillaCategory,
    "click"
  );
  moreFromMozillaCategory.click();
  await clickedPromise;

  let productCards = doc.querySelectorAll("vbox.advanced");
  Assert.ok(productCards, "The product cards from advanced template found");
  Assert.equal(productCards.length, 3, "3 product cards displayed");
  Assert.deepEqual(
    Array.from(productCards).map(
      node => node.querySelector(".product-img")?.id
    ),
    ["firefox-mobile-image", "mozilla-vpn-image", "mozilla-rally-image"],
    "Advanced template product marketing images"
  );

  let qrCodeButtons = doc.querySelectorAll('.qr-code-box[hidden="false"]');
  Assert.equal(qrCodeButtons.length, 1, "1 qr-code box displayed");

  BrowserTestUtils.removeTab(gBrowser.selectedTab);
});

add_task(async function test_aboutpreferences_clickBtnVPN() {
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

  const expectedUrl = "https://www.mozilla.org/products/vpn/";
  let tabOpened = BrowserTestUtils.waitForNewTab(gBrowser, url =>
    url.startsWith(expectedUrl)
  );
  let vpnButton = doc.getElementById("simple-mozillaVPN");
  vpnButton.doCommand();
  let openedTab = await tabOpened;
  Assert.ok(gBrowser.selectedBrowser.documentURI.spec.startsWith(expectedUrl));

  let searchParams = new URL(gBrowser.selectedBrowser.documentURI.spec)
    .searchParams;
  Assert.equal(
    searchParams.get("utm_source"),
    "about-prefs",
    "expected utm_source sent"
  );
  Assert.equal(
    searchParams.get("utm_campaign"),
    "morefrommozilla",
    "utm_campaign set"
  );
  Assert.equal(
    searchParams.get("utm_medium"),
    "firefox-desktop",
    "utm_medium set"
  );
  Assert.equal(
    searchParams.get("utm_content"),
    "fxvt-113-a-na",
    "utm_content set"
  );

  // Since we're not running MfM experiments in this release, we want to be sure that
  // our URL params aren't claiming that we are.
  Assert.ok(
    !searchParams.has("entrypoint_variation"),
    "entrypoint_variation should not be set"
  );
  Assert.ok(
    !searchParams.has("entrypoint_experiment"),
    "entrypoint_experiment should not be set"
  );

  BrowserTestUtils.removeTab(openedTab);
  BrowserTestUtils.removeTab(tab);
});

add_task(async function test_aboutpreferences_search() {
  await SpecialPowers.pushPrefEnv({
    set: [
      ["browser.preferences.moreFromMozilla", true],
      ["browser.preferences.moreFromMozilla.template", "advanced"],
    ],
  });

  await openPreferencesViaOpenPreferencesAPI(null, {
    leaveOpen: true,
  });

  await runSearchInput("Rally");

  let doc = gBrowser.contentDocument;
  let tab = gBrowser.selectedTab;

  let productCards = doc.querySelectorAll(".mozilla-product-item");
  Assert.equal(productCards.length, 3, "All products in the group are found");
  let [mobile, vpn, rally] = productCards;
  Assert.ok(BrowserTestUtils.is_hidden(mobile), "Mobile hidden");
  Assert.ok(BrowserTestUtils.is_hidden(vpn), "VPN hidden");
  Assert.ok(BrowserTestUtils.is_visible(rally), "Rally shown");

  BrowserTestUtils.removeTab(tab);
});

add_task(async function test_VPN_promo_enabled() {
  await SpecialPowers.pushPrefEnv({
    set: [["browser.vpn_promo.enabled", true]],
  });

  await openPreferencesViaOpenPreferencesAPI("paneMoreFromMozilla", {
    leaveOpen: true,
  });

  let doc = gBrowser.contentDocument;
  let vpnPromoCard = doc.getElementById("mozilla-vpn");
  let mobileCard = doc.getElementById("firefox-mobile");
  ok(vpnPromoCard, "The VPN promo is visible");
  ok(mobileCard, "The Mobile promo is visible");

  BrowserTestUtils.removeTab(gBrowser.selectedTab);
});

add_task(async function test_VPN_promo_disabled() {
  await SpecialPowers.pushPrefEnv({
    set: [["browser.vpn_promo.enabled", false]],
  });

  await openPreferencesViaOpenPreferencesAPI("paneMoreFromMozilla", {
    leaveOpen: true,
  });

  let doc = gBrowser.contentDocument;
  let vpnPromoCard = doc.getElementById("mozilla-vpn");
  let mobileCard = doc.getElementById("firefox-mobile");
  ok(!vpnPromoCard, "The VPN promo is not visible");
  ok(mobileCard, "The Mobile promo is visible");

  Services.prefs.clearUserPref("browser.vpn_promo.enabled");
  BrowserTestUtils.removeTab(gBrowser.selectedTab);
});

add_task(async function test_VPN_promo_in_disallowed_home_region() {
  const disallowedRegion = "SY";

  setupRegions(disallowedRegion);

  // Promo should not show in disallowed regions even when vpn_promo pref is enabled
  await SpecialPowers.pushPrefEnv({
    set: [["browser.vpn_promo.enabled", true]],
  });

  await openPreferencesViaOpenPreferencesAPI("paneMoreFromMozilla", {
    leaveOpen: true,
  });

  let doc = gBrowser.contentDocument;
  let vpnPromoCard = doc.getElementById("mozilla-vpn");
  let mobileCard = doc.getElementById("firefox-mobile");
  ok(!vpnPromoCard, "The VPN promo is not visible");
  ok(mobileCard, "The Mobile promo is visible");

  setupRegions(initialHomeRegion, intialCurrentRegion); // revert changes to regions
  BrowserTestUtils.removeTab(gBrowser.selectedTab);
});

add_task(async function test_VPN_promo_in_illegal_home_region() {
  const illegalRegion = "CN";

  setupRegions(illegalRegion);

  // Promo should not show in illegal regions even if the list of disallowed regions is somehow altered (though changing this preference is blocked)
  await SpecialPowers.pushPrefEnv({
    set: [["browser.vpn_promo.disallowedRegions", "SY, CU"]],
  });

  await openPreferencesViaOpenPreferencesAPI("paneMoreFromMozilla", {
    leaveOpen: true,
  });

  let doc = gBrowser.contentDocument;
  let vpnPromoCard = doc.getElementById("mozilla-vpn");
  let mobileCard = doc.getElementById("firefox-mobile");
  ok(!vpnPromoCard, "The VPN promo is not visible");
  ok(mobileCard, "The Mobile promo is visible");

  setupRegions(initialHomeRegion, intialCurrentRegion); // revert changes to regions
  BrowserTestUtils.removeTab(gBrowser.selectedTab);
});

add_task(async function test_VPN_promo_in_disallowed_current_region() {
  const allowedRegion = "US";
  const disallowedRegion = "SY";

  setupRegions(allowedRegion, disallowedRegion);

  // Promo should not show in disallowed regions even when vpn_promo pref is enabled
  await SpecialPowers.pushPrefEnv({
    set: [["browser.vpn_promo.enabled", true]],
  });

  await openPreferencesViaOpenPreferencesAPI("paneMoreFromMozilla", {
    leaveOpen: true,
  });

  let doc = gBrowser.contentDocument;
  let vpnPromoCard = doc.getElementById("mozilla-vpn");
  let mobileCard = doc.getElementById("firefox-mobile");
  ok(!vpnPromoCard, "The VPN promo is not visible");
  ok(mobileCard, "The Mobile promo is visible");

  setupRegions(initialHomeRegion, intialCurrentRegion); // revert changes to regions
  BrowserTestUtils.removeTab(gBrowser.selectedTab);
});

add_task(async function test_VPN_promo_in_illegal_current_region() {
  const allowedRegion = "US";
  const illegalRegion = "CN";

  setupRegions(allowedRegion, illegalRegion);

  // Promo should not show in illegal regions even if the list of disallowed regions is somehow altered (though changing this preference is blocked)
  await SpecialPowers.pushPrefEnv({
    set: [["browser.vpn_promo.disallowedRegions", "SY, CU"]],
  });

  await openPreferencesViaOpenPreferencesAPI("paneMoreFromMozilla", {
    leaveOpen: true,
  });

  let doc = gBrowser.contentDocument;
  let vpnPromoCard = doc.getElementById("mozilla-vpn");
  let mobileCard = doc.getElementById("firefox-mobile");
  ok(!vpnPromoCard, "The VPN promo is not visible");
  ok(mobileCard, "The Mobile promo is visible");

  setupRegions(initialHomeRegion, intialCurrentRegion); // revert changes to regions
  BrowserTestUtils.removeTab(gBrowser.selectedTab);
});

add_task(
  async function test_rally_promo_with_approved_home_region_and_language() {
    // Only show the Rally promo when US is the region and English is the langauge
    setupRegions("US");

    await openPreferencesViaOpenPreferencesAPI("paneMoreFromMozilla", {
      leaveOpen: true,
    });

    let doc = gBrowser.contentDocument;
    let rallyPromoCard = doc.getElementById("mozilla-rally");
    let mobileCard = doc.getElementById("firefox-mobile");
    ok(rallyPromoCard, "The Rally promo is visible");
    ok(mobileCard, "The Mobile promo is visible");

    setupRegions(initialHomeRegion, intialCurrentRegion); // revert changes to regions
    BrowserTestUtils.removeTab(gBrowser.selectedTab);
  }
);

add_task(async function test_rally_promo_with_unapproved_home_region() {
  setupRegions("IS");

  await openPreferencesViaOpenPreferencesAPI("paneMoreFromMozilla", {
    leaveOpen: true,
  });

  let doc = gBrowser.contentDocument;
  let rallyPromoCard = doc.getElementById("mozilla-rally");
  let mobileCard = doc.getElementById("firefox-mobile");
  ok(!rallyPromoCard, "The Rally promo is not visible");
  ok(mobileCard, "The Mobile promo is visible");

  setupRegions(initialHomeRegion, intialCurrentRegion); // revert changes to regions
  BrowserTestUtils.removeTab(gBrowser.selectedTab);
});

add_task(async function test_rally_promo_with_unapproved_current_region() {
  setupRegions("US", "IS");

  await openPreferencesViaOpenPreferencesAPI("paneMoreFromMozilla", {
    leaveOpen: true,
  });

  let doc = gBrowser.contentDocument;
  let rallyPromoCard = doc.getElementById("mozilla-rally");
  let mobileCard = doc.getElementById("firefox-mobile");
  ok(!rallyPromoCard, "The Rally promo is not visible");
  ok(mobileCard, "The Mobile promo is visible");

  setupRegions(initialHomeRegion, intialCurrentRegion); // revert changes to regions
  BrowserTestUtils.removeTab(gBrowser.selectedTab);
});

add_task(async function test_rally_promo_with_unapproved_language() {
  function setLanguage(language) {
    Services.locale.availableLocales = [language];
    Services.locale.requestedLocales = [language];
  }
  // Rally promo should be hidden in the US for languages other than English
  setupRegions("US");
  const initialLanguage = Services.locale.appLocaleAsBCP47;
  setLanguage("ko-KR");

  await openPreferencesViaOpenPreferencesAPI("paneMoreFromMozilla", {
    leaveOpen: true,
  });

  let doc = gBrowser.contentDocument;
  let rallyPromoCard = doc.getElementById("mozilla-rally");
  let mobileCard = doc.getElementById("firefox-mobile");
  ok(!rallyPromoCard, "The Rally promo is not visible");
  ok(mobileCard, "The Mobile promo is visible");

  setupRegions(initialHomeRegion, intialCurrentRegion); // revert changes to regions
  // revert changes to language
  setLanguage(initialLanguage);
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
