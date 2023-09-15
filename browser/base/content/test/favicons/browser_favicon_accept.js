/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

const ROOT = getRootDirectory(gTestPath).replace(
  "chrome://mochitest/content/",
  "http://mochi.test:8888/"
);

add_task(async () => {
  await BrowserTestUtils.withNewTab(
    { gBrowser, url: "about:blank" },
    async browser => {
      let faviconPromise = waitForFaviconMessage(true, `${ROOT}accept.sjs`);

      BrowserTestUtils.startLoadingURIString(browser, ROOT + "accept.html");
      await BrowserTestUtils.browserLoaded(browser);

      try {
        let result = await faviconPromise;
        Assert.equal(
          result.iconURL,
          `${ROOT}accept.sjs`,
          "Should have seen the icon"
        );
      } catch (e) {
        Assert.ok(false, "Favicon load failed.");
      }
    }
  );
});
