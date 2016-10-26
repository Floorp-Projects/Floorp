/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// Test the Bing Translator client against a mock Bing service, bing.sjs.

"use strict";

const kClientIdPref = "browser.translation.bing.clientIdOverride";
const kClientSecretPref = "browser.translation.bing.apiKeyOverride";

const {BingTranslator} = Cu.import("resource:///modules/translation/BingTranslator.jsm", {});
const {TranslationDocument} = Cu.import("resource:///modules/translation/TranslationDocument.jsm", {});
const {Promise} = Cu.import("resource://gre/modules/Promise.jsm", {});

add_task(function* setup() {
  Services.prefs.setCharPref(kClientIdPref, "testClient");
  Services.prefs.setCharPref(kClientSecretPref, "testSecret");

  registerCleanupFunction(function () {
    Services.prefs.clearUserPref(kClientIdPref);
    Services.prefs.clearUserPref(kClientSecretPref);
  });
});

/**
 * Checks if the translation is happening.
 */
add_task(function* test_bing_translation() {

  // Ensure the correct client id is used for authentication.
  Services.prefs.setCharPref(kClientIdPref, "testClient");

  // Loading the fixture page.
  let url = constructFixtureURL("bug1022725-fr.html");
  let tab = yield promiseTestPageLoad(url);

  // Translating the contents of the loaded tab.
  gBrowser.selectedTab = tab;
  let browser = tab.linkedBrowser;

  yield ContentTask.spawn(browser, null, function*() {
    Cu.import("resource:///modules/translation/BingTranslator.jsm");
    Cu.import("resource:///modules/translation/TranslationDocument.jsm");

    let client = new BingTranslator(
      new TranslationDocument(content.document), "fr", "en");
    let result = yield client.translate();

    // XXXmikedeboer; here you would continue the test/ content inspection.
    Assert.ok(result, "There should be a result");
  });

  gBrowser.removeTab(tab);
});

/**
 * Ensures that the BingTranslator handles out-of-valid-key response
 * correctly. Sometimes Bing Translate replies with
 * "request credentials is not in an active state" error. BingTranslator
 * should catch this error and classify it as Service Unavailable.
 *
 */
add_task(function* test_handling_out_of_valid_key_error() {

  // Simulating request from inactive subscription.
  Services.prefs.setCharPref(kClientIdPref, "testInactive");

  // Loading the fixture page.
  let url = constructFixtureURL("bug1022725-fr.html");
  let tab = yield promiseTestPageLoad(url);

  // Translating the contents of the loaded tab.
  gBrowser.selectedTab = tab;
  let browser = tab.linkedBrowser;

  yield ContentTask.spawn(browser, null, function*() {
    Cu.import("resource:///modules/translation/BingTranslator.jsm");
    Cu.import("resource:///modules/translation/TranslationDocument.jsm");

    let client = new BingTranslator(
      new TranslationDocument(content.document), "fr", "en");
    client._resetToken();
    try {
      yield client.translate();
    } catch (ex) {
      // It is alright that the translation fails.
    }
    client._resetToken();

    // Checking if the client detected service and unavailable.
    Assert.ok(client._serviceUnavailable, "Service should be detected unavailable.");
  });

  // Cleaning up.
  Services.prefs.setCharPref(kClientIdPref, "testClient");
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
  let server = Services.prefs.getCharPref("browser.translation.bing.authURL")
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
