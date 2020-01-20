// Used by JSHint:
/* global ok, is, Cu, BrowserTestUtils, add_task, gBrowser, makeTestURL, requestLongerTimeout*/
"use strict";
const { ManifestObtainer } = ChromeUtils.import(
  "resource://gre/modules/ManifestObtainer.jsm"
);

// Don't send cookies
add_task(async function() {
  const testPath = "/browser/dom/manifest/test/cookie_setter.html";
  const tabURL = `http://example.com:80${testPath}`;
  const browser = BrowserTestUtils.addTab(gBrowser, tabURL).linkedBrowser;
  await BrowserTestUtils.browserLoaded(browser);
  const { short_name } = await ManifestObtainer.browserObtainManifest(browser);
  is(short_name, "no cookie");
  const tab = gBrowser.getTabForBrowser(browser);
  gBrowser.removeTab(tab);
});

// Send cookies
add_task(async function() {
  const testPath =
    "/browser/dom/manifest/test/cookie_setter_with_credentials.html";
  const tabURL = `http://example.com:80${testPath}`;
  const browser = BrowserTestUtils.addTab(gBrowser, tabURL).linkedBrowser;
  await BrowserTestUtils.browserLoaded(browser);
  const { short_name } = await ManifestObtainer.browserObtainManifest(browser);
  is(short_name, "🍪=true");
  const tab = gBrowser.getTabForBrowser(browser);
  gBrowser.removeTab(tab);
});

// Cross origin - we go from example.com to example.org
add_task(async function() {
  const testPath =
    "/browser/dom/manifest/test/cookie_setter_with_credentials_cross_origin.html";
  const tabURL = `http://example.com:80${testPath}`;
  const browser = BrowserTestUtils.addTab(gBrowser, tabURL).linkedBrowser;
  await BrowserTestUtils.browserLoaded(browser);
  const { short_name } = await ManifestObtainer.browserObtainManifest(browser);
  is(short_name, "no cookie");
  const tab = gBrowser.getTabForBrowser(browser);
  gBrowser.removeTab(tab);
});
