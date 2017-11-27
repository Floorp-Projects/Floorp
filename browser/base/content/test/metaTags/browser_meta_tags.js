/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

const URL = "https://example.com/browser/browser/base/content/test/metaTags/meta_tags.html";

/**
 * This tests that with the page meta_tags.html, ContentMetaHandler.jsm parses
 * out the meta tags avilable and only stores the best one for description and
 * one for preview image url. In the case of this test, the best defined meta
 * tags are "og:description" and "og:image:secure_url". The list of meta tags
 * and order of preference is found in ContentMetaHandler.jsm. Because there is
 * debounce logic in ContentLinkHandler.jsm to only make one single SQL update,
 * we have to wait for some time before checking that the page info was stored.
 */
add_task(async function test_metadata() {
  const tab = await BrowserTestUtils.openNewForegroundTab(gBrowser, URL);

  // Wait until places has stored the page info
  const pageInfo = await waitForPageInfo(URL);
  is(pageInfo.description, "og:description", "got the correct description");
  is(pageInfo.previewImageURL.href, "https://test.com/og-image-secure-url.jpg", "got the correct preview image");

  await BrowserTestUtils.removeTab(tab);
  await PlacesTestUtils.clearHistory();
});

/**
 * This test is almost like the previous one except it opens a second tab to
 * make sure the extra tab does not cause the debounce logic to be skipped. If
 * incorrectly skipped, the updated metadata would not include the delayed meta.
 */
add_task(async function multiple_tabs() {
  const tab = await BrowserTestUtils.openNewForegroundTab(gBrowser, URL);

  // Add a background tab to cause another page to load *without* putting the
  // desired URL in a background tab, which results in its timers being throttled.
  gBrowser.addTab();

  // Wait until places has stored the page info
  const pageInfo = await waitForPageInfo(URL);
  is(pageInfo.description, "og:description", "got the correct description");
  is(pageInfo.previewImageURL.href, "https://test.com/og-image-secure-url.jpg", "got the correct preview image");

  await BrowserTestUtils.removeTab(tab);
  await BrowserTestUtils.removeTab(gBrowser.selectedTab);
  await PlacesTestUtils.clearHistory();
});
