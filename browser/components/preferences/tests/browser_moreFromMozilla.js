/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const { ContentTaskUtils } = ChromeUtils.import(
  "resource://testing-common/ContentTaskUtils.jsm"
);

let { TelemetryTestUtils } = ChromeUtils.import(
  "resource://testing-common/TelemetryTestUtils.jsm"
);

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

  let productCard = doc.querySelector("vbox.list-item");
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

  let productCards = doc.querySelectorAll("vbox.simple");
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
    searchParams.get("entrypoint_experiment"),
    "morefrommozilla-experiment-1846",
    "entrypoint_experiment set"
  );
  Assert.equal(
    searchParams.get("utm_content"),
    "fxvt-113-a-na",
    "utm_content set"
  );
  Assert.equal(
    searchParams.get("entrypoint_variation"),
    "treatment-simple",
    "entrypoint_variation set"
  );

  BrowserTestUtils.removeTab(openedTab);
  BrowserTestUtils.removeTab(tab);
});
