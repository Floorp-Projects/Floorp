/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

const ROOT =
  "http://mochi.test:8888/browser/browser/base/content/test/favicons/";

add_task(async () => {
  await BrowserTestUtils.withNewTab(
    { gBrowser, url: "about:blank" },
    async browser => {
      let faviconPromise = waitForFaviconMessage(true, `${ROOT}large.png`);

      BrowserTestUtils.startLoadingURIString(
        browser,
        ROOT + "large_favicon.html"
      );
      await BrowserTestUtils.browserLoaded(browser);

      await Assert.rejects(
        faviconPromise,
        result => {
          return result.iconURL == `${ROOT}large.png`;
        },
        "Should have failed to load the large icon."
      );
    }
  );
});
