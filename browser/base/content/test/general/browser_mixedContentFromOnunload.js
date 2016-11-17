/*
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 *
 * Tests for Bug 947079 - Fix bug in nsSecureBrowserUIImpl that sets the wrong
 * security state on a page because of a subresource load that is not on the
 * same page.
 */

// We use different domains for each test and for navigation within each test
const gHttpTestRoot1 = "http://example.com/browser/browser/base/content/test/general/";
const gHttpsTestRoot1 = "https://test1.example.com/browser/browser/base/content/test/general/";
const gHttpTestRoot2 = "http://example.net/browser/browser/base/content/test/general/";
const gHttpsTestRoot2 = "https://test2.example.com/browser/browser/base/content/test/general/";

var gTestBrowser = null;
add_task(function *() {
  let url = gHttpTestRoot1 + "file_mixedContentFromOnunload.html";
  yield BrowserTestUtils.withNewTab({gBrowser, url}, function*() {
    yield new Promise(resolve => {
      SpecialPowers.pushPrefEnv({
        "set": [
          ["security.mixed_content.block_active_content", true],
          ["security.mixed_content.block_display_content", false]
        ]
      }, resolve);
    });
  gTestBrowser = gBrowser.selectedBrowser;
  // Navigation from an http page to a https page with no mixed content
  // The http page loads an http image on unload
  url = gHttpsTestRoot1 + "file_mixedContentFromOnunload_test1.html";
  yield BrowserTestUtils.loadURI(gTestBrowser, url);
  yield BrowserTestUtils.browserLoaded(gTestBrowser);
  // check security state.  Since current url is https and doesn't have any
  // mixed content resources, we expect it to be secure.
  isSecurityState("secure");
  assertMixedContentBlockingState(gTestBrowser, {activeLoaded: false, activeBlocked: false, passiveLoaded: false});
  // Navigation from an http page to a https page that has mixed display content
  // The https page loads an http image on unload
  url = gHttpTestRoot2 + "file_mixedContentFromOnunload.html";
  yield BrowserTestUtils.loadURI(gTestBrowser, url);
  yield BrowserTestUtils.browserLoaded(gTestBrowser);
  url = gHttpsTestRoot2 + "file_mixedContentFromOnunload_test2.html";
  yield BrowserTestUtils.loadURI(gTestBrowser, url);
  yield BrowserTestUtils.browserLoaded(gTestBrowser);
  isSecurityState("broken");
  assertMixedContentBlockingState(gTestBrowser, {activeLoaded: false, activeBlocked: false, passiveLoaded: true});
  });
});
