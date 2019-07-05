/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

add_task(async () => {
  const testPath =
    "http://example.com/browser/browser/base/content/test/favicons/";
  const expectedIcon = "http://example.com/favicon.ico";

  await BrowserTestUtils.withNewTab(
    { gBrowser, url: "about:blank" },
    async browser => {
      let faviconPromise = waitForLinkAvailable(browser);
      BrowserTestUtils.loadURI(browser, testPath + "file_invalid_href.html");
      await BrowserTestUtils.browserLoaded(browser);

      let iconURI = await faviconPromise;
      Assert.equal(
        iconURI,
        expectedIcon,
        "Should have fallen back to the default site favicon for an invalid href attribute"
      );
    }
  );
});
