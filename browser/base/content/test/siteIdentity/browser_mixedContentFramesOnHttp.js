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

const TEST_URL = getRootDirectory(gTestPath).replace("chrome://mochitests/content", "http://example.com") + "file_mixedContentFramesOnHttp.html";

add_task(function *() {
  yield SpecialPowers.pushPrefEnv({
    "set": [
      ["security.mixed_content.block_active_content", true],
      ["security.mixed_content.block_display_content", false]
    ]});

  yield BrowserTestUtils.withNewTab(TEST_URL, function*(browser) {
    isSecurityState(browser, "insecure");
    assertMixedContentBlockingState(browser, {activeLoaded: false, activeBlocked: false, passiveLoaded: true});
  });
});

