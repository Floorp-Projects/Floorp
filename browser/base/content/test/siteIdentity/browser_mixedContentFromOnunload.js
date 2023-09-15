/*
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 *
 * Tests for Bug 947079 - Fix bug in nsSecureBrowserUIImpl that sets the wrong
 * security state on a page because of a subresource load that is not on the
 * same page.
 */

// We use different domains for each test and for navigation within each test
const HTTP_TEST_ROOT_1 = getRootDirectory(gTestPath).replace(
  "chrome://mochitests/content",
  // eslint-disable-next-line @microsoft/sdl/no-insecure-url
  "http://example.com"
);
const HTTPS_TEST_ROOT_1 = getRootDirectory(gTestPath).replace(
  "chrome://mochitests/content",
  "https://test1.example.com"
);
const HTTP_TEST_ROOT_2 = getRootDirectory(gTestPath).replace(
  "chrome://mochitests/content",
  // eslint-disable-next-line @microsoft/sdl/no-insecure-url
  "http://example.net"
);
const HTTPS_TEST_ROOT_2 = getRootDirectory(gTestPath).replace(
  "chrome://mochitests/content",
  "https://test2.example.com"
);

add_task(async function () {
  let url = HTTP_TEST_ROOT_1 + "file_mixedContentFromOnunload.html";
  await BrowserTestUtils.withNewTab(url, async function (browser) {
    await SpecialPowers.pushPrefEnv({
      set: [
        ["security.mixed_content.block_active_content", true],
        ["security.mixed_content.block_display_content", false],
        ["security.mixed_content.upgrade_display_content", false],
      ],
    });
    // Navigation from an http page to a https page with no mixed content
    // The http page loads an http image on unload
    url = HTTPS_TEST_ROOT_1 + "file_mixedContentFromOnunload_test1.html";
    BrowserTestUtils.startLoadingURIString(browser, url);
    await BrowserTestUtils.browserLoaded(browser);
    // check security state.  Since current url is https and doesn't have any
    // mixed content resources, we expect it to be secure.
    isSecurityState(browser, "secure");
    await assertMixedContentBlockingState(browser, {
      activeLoaded: false,
      activeBlocked: false,
      passiveLoaded: false,
    });
    // Navigation from an http page to a https page that has mixed display content
    // The https page loads an http image on unload
    url = HTTP_TEST_ROOT_2 + "file_mixedContentFromOnunload.html";
    BrowserTestUtils.startLoadingURIString(browser, url);
    await BrowserTestUtils.browserLoaded(browser);
    url = HTTPS_TEST_ROOT_2 + "file_mixedContentFromOnunload_test2.html";
    BrowserTestUtils.startLoadingURIString(browser, url);
    await BrowserTestUtils.browserLoaded(browser);
    isSecurityState(browser, "broken");
    await assertMixedContentBlockingState(browser, {
      activeLoaded: false,
      activeBlocked: false,
      passiveLoaded: true,
    });
  });
});
