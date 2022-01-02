/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// Test the Bing Translator client against a mock Bing service, bing.sjs.

"use strict";

const kClientIdPref = "browser.translation.bing.clientIdOverride";
const kClientSecretPref = "browser.translation.bing.apiKeyOverride";

const { BingTranslator } = ChromeUtils.import(
  "resource:///modules/translation/BingTranslator.jsm"
);
const { TranslationDocument } = ChromeUtils.import(
  "resource:///modules/translation/TranslationDocument.jsm"
);

add_task(async function setup() {
  Services.prefs.setCharPref(kClientIdPref, "testClient");
  Services.prefs.setCharPref(kClientSecretPref, "testSecret");

  registerCleanupFunction(function() {
    Services.prefs.clearUserPref(kClientIdPref);
    Services.prefs.clearUserPref(kClientSecretPref);
  });
});

/**
 * Checks if the translation is happening.
 */
add_task(async function test_bing_translation() {
  // Ensure the correct client id is used for authentication.
  Services.prefs.setCharPref(kClientIdPref, "testClient");

  // Loading the fixture page.
  let url = constructFixtureURL("bug1022725-fr.html");
  let tab = await promiseTestPageLoad(url);

  // Translating the contents of the loaded tab.
  gBrowser.selectedTab = tab;
  let browser = tab.linkedBrowser;

  await SpecialPowers.spawn(browser, [], async function() {
    // eslint-disable-next-line no-shadow
    const { BingTranslator } = ChromeUtils.import(
      "resource:///modules/translation/BingTranslator.jsm"
    );
    // eslint-disable-next-line no-shadow
    const { TranslationDocument } = ChromeUtils.import(
      "resource:///modules/translation/TranslationDocument.jsm"
    );

    let client = new BingTranslator(
      new TranslationDocument(content.document),
      "fr",
      "en"
    );
    let result = await client.translate();

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
add_task(async function test_handling_out_of_valid_key_error() {
  // Simulating request from inactive subscription.
  Services.prefs.setCharPref(kClientIdPref, "testInactive");

  // Loading the fixture page.
  let url = constructFixtureURL("bug1022725-fr.html");
  let tab = await promiseTestPageLoad(url);

  // Translating the contents of the loaded tab.
  gBrowser.selectedTab = tab;
  let browser = tab.linkedBrowser;

  await SpecialPowers.spawn(browser, [], async function() {
    // eslint-disable-next-line no-shadow
    const { BingTranslator } = ChromeUtils.import(
      "resource:///modules/translation/BingTranslator.jsm"
    );
    // eslint-disable-next-line no-shadow
    const { TranslationDocument } = ChromeUtils.import(
      "resource:///modules/translation/TranslationDocument.jsm"
    );

    let client = new BingTranslator(
      new TranslationDocument(content.document),
      "fr",
      "en"
    );
    client._resetToken();
    try {
      await client.translate();
    } catch (ex) {
      // It is alright that the translation fails.
    }
    client._resetToken();

    // Checking if the client detected service and unavailable.
    Assert.ok(
      client._serviceUnavailable,
      "Service should be detected unavailable."
    );
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
  let server = Services.prefs
    .getCharPref("browser.translation.bing.authURL")
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
