/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// Test the Yandex Translator client against a mock Yandex service, yandex.sjs.

"use strict";

// The folllowing rejection is left unhandled in some cases. This bug should be
// fixed, but for the moment this file allows a class of rejections.
//
// NOTE: Allowing a whole class of rejections should be avoided. Normally you
//       should use "expectUncaughtRejection" to flag individual failures.
const { PromiseTestUtils } = ChromeUtils.import(
  "resource://testing-common/PromiseTestUtils.jsm"
);
PromiseTestUtils.allowMatchingRejectionsGlobally(/NS_ERROR_ILLEGAL_VALUE/);

const kEnginePref = "browser.translation.engine";
const kApiKeyPref = "browser.translation.yandex.apiKeyOverride";
const kDetectLanguagePref = "browser.translation.detectLanguage";
const kShowUIPref = "browser.translation.ui.show";

const { Translation } = ChromeUtils.import(
  "resource:///modules/translation/TranslationParent.jsm"
);

add_task(async function setup() {
  await SpecialPowers.pushPrefEnv({
    set: [
      [kEnginePref, "Yandex"],
      [kApiKeyPref, "yandexValidKey"],
      [kDetectLanguagePref, true],
      [kShowUIPref, true],
    ],
  });
});

/**
 * Ensure that the translation engine behaives as expected when translating
 * a sample page.
 */
add_task(async function test_yandex_translation() {
  // Loading the fixture page.
  let url = constructFixtureURL("bug1022725-fr.html");
  let tab = await promiseTestPageLoad(url);

  // Translating the contents of the loaded tab.
  gBrowser.selectedTab = tab;
  let browser = tab.linkedBrowser;

  await SpecialPowers.spawn(browser, [], async function() {
    const { TranslationDocument } = ChromeUtils.import(
      "resource:///modules/translation/TranslationDocument.jsm"
    );
    const { YandexTranslator } = ChromeUtils.import(
      "resource:///modules/translation/YandexTranslator.jsm"
    );

    let client = new YandexTranslator(
      new TranslationDocument(content.document),
      "fr",
      "en"
    );
    let result = await client.translate();

    Assert.ok(result, "There should be a result.");
  });

  gBrowser.removeTab(tab);
});

/**
 * Ensure that Yandex.Translate is propertly attributed.
 */
add_task(async function test_yandex_attribution() {
  // Loading the fixture page.
  let url = constructFixtureURL("bug1022725-fr.html");
  let tab = await promiseTestPageLoad(url);

  info("Show an info bar saying the current page is in French");
  let notif = showTranslationUI(tab, "fr");
  let attribution = notif._getAnonElt("translationEngine").selectedIndex;
  Assert.equal(attribution, 1, "Yandex attribution should be shown.");

  gBrowser.removeTab(tab);
});

add_task(async function test_preference_attribution() {
  let prefUrl = "about:preferences#general";
  let waitPrefLoaded = TestUtils.topicObserved("sync-pane-loaded", () => true);
  let tab = await promiseTestPageLoad(prefUrl);
  await waitPrefLoaded;
  let browser = gBrowser.getBrowserForTab(tab);
  let win = browser.contentWindow;
  let bingAttribution = win.document.getElementById("bingAttribution");
  ok(bingAttribution, "Bing attribution should exist.");
  ok(bingAttribution.hidden, "Bing attribution should be hidden.");

  gBrowser.removeTab(tab);
});

/**
 * A helper function for constructing a URL to a page stored in the
 * local fixture folder.
 *
 * @param filename  Name of a fixture file.
 */
function constructFixtureURL(filename) {
  // Deduce the Mochitest server address in use from a pref that was pre-processed.
  let server = Services.prefs
    .getCharPref("browser.translation.yandex.translateURLOverride")
    .replace("http://", "");
  server = server.substr(0, server.indexOf("/"));
  let url =
    "http://" +
    server +
    "/browser/browser/components/translation/test/fixtures/" +
    filename;
  return url;
}

/**
 * A helper function to open a new tab and wait for its content to load.
 *
 * @param String url  A URL to be loaded in the new tab.
 */
function promiseTestPageLoad(url) {
  return new Promise(resolve => {
    let tab = (gBrowser.selectedTab = BrowserTestUtils.addTab(gBrowser, url));
    let browser = gBrowser.selectedBrowser;
    BrowserTestUtils.browserLoaded(
      browser,
      false,
      loadurl => loadurl != "about:blank"
    ).then(() => {
      info("Page loaded: " + browser.currentURI.spec);
      resolve(tab);
    });
  });
}

function showTranslationUI(tab, aDetectedLanguage) {
  let browser = gBrowser.selectedBrowser;
  let actor = browser.browsingContext.currentWindowGlobal.getActor(
    "Translation"
  );
  actor.documentStateReceived({
    state: Translation.STATE_OFFER,
    originalShown: true,
    detectedLanguage: aDetectedLanguage,
  });

  return actor.notificationBox.getNotificationWithValue("translation");
}
