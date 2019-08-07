// Used by JSHint:
/* global ok, is, Cu, BrowserTestUtils, add_task, gBrowser, makeTestURL, requestLongerTimeout*/
"use strict";
const { ManifestObtainer } = ChromeUtils.import(
  "resource://gre/modules/ManifestObtainer.jsm"
);

add_task(async function() {
  const testPath = "/browser/dom/manifest/test/cookie_setter.html";
  const tabURL = `http://example.com:80${testPath}`;
  const browser = BrowserTestUtils.addTab(gBrowser, tabURL).linkedBrowser;
  await BrowserTestUtils.browserLoaded(browser);
  const { short_name } = await ManifestObtainer.browserObtainManifest(browser);
  is(short_name, "PASS", `Expected 'PASS', but got ${short_name}.`);
  const tab = gBrowser.getTabForBrowser(browser);
  gBrowser.removeTab(tab);
});
