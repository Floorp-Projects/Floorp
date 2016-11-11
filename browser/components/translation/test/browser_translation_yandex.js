/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// Test the Yandex Translator client against a mock Yandex service, yandex.sjs.

"use strict";

const kEnginePref = "browser.translation.engine";
const kApiKeyPref = "browser.translation.yandex.apiKeyOverride";
const kShowUIPref = "browser.translation.ui.show";

const {Promise} = Cu.import("resource://gre/modules/Promise.jsm", {});
const {Translation} = Cu.import("resource:///modules/translation/Translation.jsm", {});

add_task(function* setup() {
  Services.prefs.setCharPref(kEnginePref, "yandex");
  Services.prefs.setCharPref(kApiKeyPref, "yandexValidKey");
  Services.prefs.setBoolPref(kShowUIPref, true);

  registerCleanupFunction(function() {
    Services.prefs.clearUserPref(kEnginePref);
    Services.prefs.clearUserPref(kApiKeyPref);
    Services.prefs.clearUserPref(kShowUIPref);
  });
});

/**
 * Ensure that the translation engine behaives as expected when translating
 * a sample page.
 */
add_task(function* test_yandex_translation() {

  // Loading the fixture page.
  let url = constructFixtureURL("bug1022725-fr.html");
  let tab = yield promiseTestPageLoad(url);

  // Translating the contents of the loaded tab.
  gBrowser.selectedTab = tab;
  let browser = tab.linkedBrowser;

  yield ContentTask.spawn(browser, null, function*() {
    Cu.import("resource:///modules/translation/TranslationDocument.jsm");
    Cu.import("resource:///modules/translation/YandexTranslator.jsm");

    let client = new YandexTranslator(
      new TranslationDocument(content.document), "fr", "en");
    let result = yield client.translate();

    Assert.ok(result, "There should be a result.");
  });

  gBrowser.removeTab(tab);
});

/**
 * Ensure that Yandex.Translate is propertly attributed.
 */
add_task(function* test_yandex_attribution() {
  // Loading the fixture page.
  let url = constructFixtureURL("bug1022725-fr.html");
  let tab = yield promiseTestPageLoad(url);

  info("Show an info bar saying the current page is in French");
  let notif = showTranslationUI(tab, "fr");
  let attribution = notif._getAnonElt("translationEngine").selectedIndex;
  Assert.equal(attribution, 1, "Yandex attribution should be shown.");

  gBrowser.removeTab(tab);
});


add_task(function* test_preference_attribution() {

    let prefUrl = "about:preferences#content";
    let tab = yield promiseTestPageLoad(prefUrl);

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
  let server = Services.prefs.getCharPref("browser.translation.yandex.translateURLOverride")
                             .replace("http://", "");
  server = server.substr(0, server.indexOf("/"));
  let url = "http://" + server +
    "/browser/browser/components/translation/test/fixtures/" + filename;
  return url;
}

/**
 * A helper function to open a new tab and wait for its content to load.
 *
 * @param String url  A URL to be loaded in the new tab.
 */
function promiseTestPageLoad(url) {
  let deferred = Promise.defer();
  let tab = gBrowser.selectedTab = gBrowser.addTab(url);
  let browser = gBrowser.selectedBrowser;
  browser.addEventListener("load", function listener() {
    if (browser.currentURI.spec == "about:blank")
      return;
    info("Page loaded: " + browser.currentURI.spec);
    browser.removeEventListener("load", listener, true);
    deferred.resolve(tab);
  }, true);
  return deferred.promise;
}

function showTranslationUI(tab, aDetectedLanguage) {
  let browser = gBrowser.selectedBrowser;
  Translation.documentStateReceived(browser, {state: Translation.STATE_OFFER,
                                              originalShown: true,
                                              detectedLanguage: aDetectedLanguage});
  let ui = browser.translationUI;
  return ui.notificationBox.getNotificationWithValue("translation");
}
