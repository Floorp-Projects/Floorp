/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// Test the Bing Translator client against a mock Bing service, bing.sjs.

"use strict";

const kClientIdPref = "browser.translation.bing.clientIdOverride";
const kClientSecretPref = "browser.translation.bing.apiKeyOverride";

const {BingTranslator} = Cu.import("resource:///modules/translation/BingTranslator.jsm", {});
const {TranslationDocument} = Cu.import("resource:///modules/translation/TranslationDocument.jsm", {});

function test() {
  waitForExplicitFinish();

  Services.prefs.setCharPref(kClientIdPref, "testClient");
  Services.prefs.setCharPref(kClientSecretPref, "testSecret");

  // Deduce the Mochitest server address in use from a pref that was pre-processed.
  let server = Services.prefs.getCharPref("browser.translation.bing.authURL")
                             .replace("http://", "");
  server = server.substr(0, server.indexOf("/"));
  let tab = gBrowser.addTab("http://" + server +
    "/browser/browser/components/translation/test/fixtures/bug1022725-fr.html");
  gBrowser.selectedTab = tab;

  registerCleanupFunction(function () {
    gBrowser.removeTab(tab);
    Services.prefs.clearUserPref(kClientIdPref);
    Services.prefs.clearUserPref(kClientSecretPref);
  });

  let browser = tab.linkedBrowser;
  browser.addEventListener("load", function onload() {
    if (browser.currentURI.spec == "about:blank")
      return;

    browser.removeEventListener("load", onload, true);
    let client = new BingTranslator(
      new TranslationDocument(browser.contentDocument), "fr", "en");

    client.translate().then(
      result => {
        // XXXmikedeboer; here you would continue the test/ content inspection.
        ok(result, "There should be a result.");
        finish();
      },
      error => {
        ok(false, "Unexpected Client Error: " + error);
        finish();
      }
    );
  }, true);
}
