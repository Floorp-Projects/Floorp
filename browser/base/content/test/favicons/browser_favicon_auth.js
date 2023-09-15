/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

const ROOT = getRootDirectory(gTestPath).replace(
  "chrome://mochitests/content/",
  "http://mochi.test:8888/"
);

add_task(async () => {
  await BrowserTestUtils.withNewTab(
    { gBrowser, url: "about:blank" },
    async browser => {
      let faviconPromise = waitForFaviconMessage(true, `${ROOT}auth_test.png`);

      BrowserTestUtils.startLoadingURIString(browser, `${ROOT}auth_test.html`);
      await BrowserTestUtils.browserLoaded(browser);

      await Assert.rejects(
        faviconPromise,
        result => {
          return result.iconURL == `${ROOT}auth_test.png`;
        },
        "Should have failed to load the icon."
      );
    }
  );
});
