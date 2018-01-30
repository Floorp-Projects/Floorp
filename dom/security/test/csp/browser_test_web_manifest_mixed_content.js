/*
 * Description of the test:
 *   Check that mixed content blocker works prevents fetches of
 *   mixed content manifests.
 */
/*globals Cu, ok*/
"use strict";
const {
  ManifestObtainer
} = Cu.import("resource://gre/modules/ManifestObtainer.jsm", {});
const path = "/tests/dom/security/test/csp/";
const mixedContent = `${path}file_web_manifest_mixed_content.html`;
const server = `${path}file_testserver.sjs`;
const secureURL = new URL(`https://example.com${server}`);
const tests = [
  // Trying to load mixed content in file_web_manifest_mixed_content.html
  // needs to result in an error.
  {
    expected: "Mixed Content Blocker prevents fetching manifest.",
    get tabURL() {
      const url = new URL(secureURL);
      url.searchParams.append("file", mixedContent);
      return url.href;
    },
    run(error) {
      // Check reason for error.
      const check = /NetworkError when attempting to fetch resource/.test(error.message);
      ok(check, this.expected);
    }
  }
];

//jscs:disable
add_task(async function() {
  //jscs:enable
  const testPromises = tests.map((test) => {
    const tabOptions = {
      gBrowser,
      url: test.tabURL,
      skipAnimation: true,
    };
    return BrowserTestUtils.withNewTab(tabOptions, (browser) => testObtainingManifest(browser, test));
  });
  await Promise.all(testPromises);
});

async function testObtainingManifest(aBrowser, aTest) {
  try {
    await ManifestObtainer.browserObtainManifest(aBrowser);
  } catch (e) {
    aTest.run(e);
  }
}
