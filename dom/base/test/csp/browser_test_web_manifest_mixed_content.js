/*
 * Description of the test:
 *   Check that mixed content blocker works prevents fetches of
 *   mixed content manifests.
 */
'use strict';
const {
  ManifestObtainer
} = Components.utils.import('resource://gre/modules/WebManifest.jsm', {});
const path = '/tests/dom/base/test/csp/';
const mixedContent = `file=${path}file_CSP_web_manifest_mixed_content.html`;
const server = 'file_csp_testserver.sjs';
const secureURL = `https://example.com${path}${server}`;
const tests = [
  // Trying to load mixed content in file_CSP_web_manifest_mixed_content.html
  // needs to result in an error.
  {
    expected: `Mixed Content Blocker prevents fetching manifest.`,
    get tabURL() {
      let queryParts = [
        mixedContent
      ];
      return `${secureURL}?${queryParts.join('&')}`;
    },
    run(error) {
      // Check reason for error.
      const check = /blocked the loading of a resource/.test(error.message);
      ok(check, this.expected);
    }
  }
];

//jscs:disable
add_task(function*() {
  //jscs:enable
  for (let test of tests) {
    let tabOptions = {
      gBrowser: gBrowser,
      url: test.tabURL,
    };
    yield BrowserTestUtils.withNewTab(
      tabOptions,
      browser => testObtainingManifest(browser, test)
    );
  }

  function* testObtainingManifest(aBrowser, aTest) {
    const obtainer = new ManifestObtainer();
    try {
      yield obtainer.obtainManifest(aBrowser);
    } catch (e) {
      aTest.run(e)
    }
  }
});
