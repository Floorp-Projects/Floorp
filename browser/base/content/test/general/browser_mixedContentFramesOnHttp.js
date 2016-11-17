/*
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 *
 * Test for Bug 1182551 -
 *
 * This test has a top level HTTP page with an HTTPS iframe.  The HTTPS iframe
 * includes an HTTP image.  We check that the top level security state is
 * STATE_IS_INSECURE.  The mixed content from the iframe shouldn't "upgrade"
 * the HTTP top level page to broken HTTPS.
 */

const gHttpTestUrl = "http://example.com/browser/browser/base/content/test/general/file_mixedContentFramesOnHttp.html";

var gTestBrowser = null;

add_task(function *() {
  yield SpecialPowers.pushPrefEnv({
    "set": [
      ["security.mixed_content.block_active_content", true],
      ["security.mixed_content.block_display_content", false]
    ]});

  let url = gHttpTestUrl
  yield BrowserTestUtils.withNewTab({gBrowser, url}, function*() {
    gTestBrowser = gBrowser.selectedBrowser;
    // check security state is insecure
    isSecurityState("insecure");
    assertMixedContentBlockingState(gTestBrowser, {activeLoaded: false, activeBlocked: false, passiveLoaded: true});
  });
});

