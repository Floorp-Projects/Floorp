/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const { EnterprisePolicyTesting } = ChromeUtils.importESModule(
  "resource://testing-common/EnterprisePolicyTesting.sys.mjs"
);

let { TelemetryTestUtils } = ChromeUtils.importESModule(
  "resource://testing-common/TelemetryTestUtils.sys.mjs"
);

const TOTAL_PROMO_CARDS_COUNT = 4;

async function clearPolicies() {
  // Ensure no active policies are set
  await EnterprisePolicyTesting.setupPolicyEngineWithJson("");
}

// The Relay promo is only shown if the default FxA instance is detected, and
// tests override it to a dummy address, so we need to make the dummy address
// appear like it's the default (using the actual default instance might cause a
// remote connection, crashing the test harness).
add_setup(mockDefaultFxAInstance);

add_task(async function testDefaultUIWithoutTemplatePref() {
  await clearPolicies();
  await openPreferencesViaOpenPreferencesAPI("paneGeneral", {
    leaveOpen: true,
  });
  let doc = gBrowser.contentDocument;
  let tab = gBrowser.selectedTab;

  let moreFromMozillaCategory = doc.getElementById(
    "category-more-from-mozilla"
  );
  ok(moreFromMozillaCategory, "The category exists");
  ok(!moreFromMozillaCategory.hidden, "The category is not hidden");

  moreFromMozillaCategory.click();

  let productCards = doc.querySelectorAll(".mozilla-product-item.simple");
  Assert.ok(productCards, "Default UI uses simple template");
  Assert.equal(
    productCards.length,
    TOTAL_PROMO_CARDS_COUNT,
    "All product cards displayed"
  );

  const expectedUrl = "https://www.mozilla.org/firefox/browsers/mobile/";
  let tabOpened = BrowserTestUtils.waitForNewTab(gBrowser, url =>
    url.startsWith(expectedUrl)
  );
  let mobileLink = doc.getElementById("default-fxMobile");
  mobileLink.click();
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
    "default-global",
    "default utm_content set"
  );
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

add_task(async function testDefaulEmailClick() {
  await clearPolicies();
  await openPreferencesViaOpenPreferencesAPI("paneGeneral", {
    leaveOpen: true,
  });
  let doc = gBrowser.contentDocument;
  let tab = gBrowser.selectedTab;

  let moreFromMozillaCategory = doc.getElementById(
    "category-more-from-mozilla"
  );
  moreFromMozillaCategory.click();

  const expectedUrl = "https://www.mozilla.org/firefox/mobile/get-app/?v=mfm";
  let sendEmailLink = doc.getElementById("default-qr-code-send-email");

  Assert.ok(
    sendEmailLink.href.startsWith(expectedUrl),
    `Expected URL ${sendEmailLink.href}`
  );

  let searchParams = new URL(sendEmailLink.href).searchParams;
  Assert.equal(searchParams.get("v"), "mfm", "expected send email param set");
  BrowserTestUtils.removeTab(tab);
});

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

  let clickedPromise = BrowserTestUtils.waitForEvent(
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
  await clearPolicies();
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

  moreFromMozillaCategory.click();

  let productCards = doc.querySelectorAll(".mozilla-product-item");
  Assert.ok(productCards, "The product cards from simple template found");
  Assert.equal(
    productCards.length,
    TOTAL_PROMO_CARDS_COUNT,
    "All product cards displayed"
  );

  let qrCodeButtons = doc.querySelectorAll('.qr-code-box[hidden="false"]');
  Assert.equal(qrCodeButtons.length, 1, "1 qr-code box displayed");

  BrowserTestUtils.removeTab(gBrowser.selectedTab);
});

add_task(async function test_aboutpreferences_clickBtnVPN() {
  await clearPolicies();
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

  let productCards = doc.querySelectorAll(".mozilla-product-item.simple");
  Assert.ok(productCards, "Simple template loaded");

  const expectedUrl = "https://www.mozilla.org/products/vpn/";
  let tabOpened = BrowserTestUtils.waitForNewTab(gBrowser, url =>
    url.startsWith(expectedUrl)
  );

  let vpnButton = doc.getElementById("simple-mozillaVPN");
  vpnButton.click();

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
    "fxvt-113-a-global",
    "utm_content set"
  );
  Assert.equal(
    searchParams.get("entrypoint_experiment"),
    "morefrommozilla-experiment-1846",
    "entrypoint_experiment set"
  );
  Assert.equal(
    searchParams.get("entrypoint_variation"),
    "treatment-simple",
    "entrypoint_variation set"
  );
  BrowserTestUtils.removeTab(openedTab);
  BrowserTestUtils.removeTab(tab);
});

add_task(async function test_aboutpreferences_clickBtnMobile() {
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

  const expectedUrl = "https://www.mozilla.org/firefox/browsers/mobile/";

  let mobileUrl = new URL(doc.getElementById("simple-fxMobile").href);

  Assert.ok(mobileUrl.href.startsWith(expectedUrl));

  let searchParams = mobileUrl.searchParams;
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
    "fxvt-113-a-global",
    "default-global",
    "utm_content set"
  );

  BrowserTestUtils.removeTab(tab);
});

add_task(async function test_aboutpreferences_search() {
  await clearPolicies();
  await SpecialPowers.pushPrefEnv({
    set: [["browser.preferences.moreFromMozilla", true]],
  });

  await openPreferencesViaOpenPreferencesAPI(null, {
    leaveOpen: true,
  });

  await runSearchInput("Relay");

  let doc = gBrowser.contentDocument;
  let tab = gBrowser.selectedTab;

  let productCards = doc.querySelectorAll(".mozilla-product-item");
  Assert.equal(
    productCards.length,
    TOTAL_PROMO_CARDS_COUNT,
    "All products in the group are found"
  );
  let [mobile, monitor, vpn, relay] = productCards;
  Assert.ok(BrowserTestUtils.isHidden(mobile), "Mobile hidden");
  Assert.ok(BrowserTestUtils.isHidden(monitor), "Monitor hidden");
  Assert.ok(BrowserTestUtils.isHidden(vpn), "VPN hidden");
  Assert.ok(BrowserTestUtils.isVisible(relay), "Relay shown");

  BrowserTestUtils.removeTab(tab);
});

add_task(async function test_aboutpreferences_clickBtnRelay() {
  await SpecialPowers.pushPrefEnv({
    set: [["browser.preferences.moreFromMozilla", true]],
  });
  await openPreferencesViaOpenPreferencesAPI("paneMoreFromMozilla", {
    leaveOpen: true,
  });

  let doc = gBrowser.contentDocument;
  let tab = gBrowser.selectedTab;

  let expectedUrl = new URL("https://relay.firefox.com");
  expectedUrl.searchParams.set("utm_source", "about-prefs");
  expectedUrl.searchParams.set("utm_campaign", "morefrommozilla");
  expectedUrl.searchParams.set("utm_medium", "firefox-desktop");
  expectedUrl.searchParams.set("utm_content", "fxvt-113-a-global");
  expectedUrl.searchParams.set(
    "entrypoint_experiment",
    "morefrommozilla-experiment-1846"
  );
  expectedUrl.searchParams.set("entrypoint_variation", "treatment-simple");

  let tabOpened = BrowserTestUtils.waitForDocLoadAndStopIt(
    expectedUrl.toString(),
    gBrowser,
    channel => {
      Assert.equal(
        channel.originalURI.spec,
        expectedUrl.toString(),
        "URL matched"
      );
      return true;
    }
  );
  doc.getElementById("simple-firefoxRelay").click();

  await tabOpened;
  BrowserTestUtils.removeTab(gBrowser.selectedTab);
  BrowserTestUtils.removeTab(tab);
});
