/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// The test loads an initial page and then another page which does enough
// fragment navigations so that when going back to the initial page and then
// forward to the last page, the initial page is evicted from the bfcache.
add_task(async function testBFCacheEviction() {
  // Make an unrealistic large timeout.
  await SpecialPowers.pushPrefEnv({
    set: [["browser.sessionhistory.contentViewerTimeout", 86400]],
  });

  const uri = "data:text/html,initial page";
  const uri2 =
    getRootDirectory(gTestPath).replace(
      "chrome://mochitests/content",
      "https://example.com"
    ) + "dummy_page.html";

  await BrowserTestUtils.withNewTab(
    { gBrowser, url: uri },
    async function (browser) {
      BrowserTestUtils.startLoadingURIString(browser, uri2);
      await BrowserTestUtils.browserLoaded(browser);

      let awaitPageShow = BrowserTestUtils.waitForContentEvent(
        browser,
        "pageshow"
      );
      await SpecialPowers.spawn(browser, [], async function () {
        content.location.hash = "1";
        content.location.hash = "2";
        content.location.hash = "3";
        content.history.go(-4);
      });

      await awaitPageShow;

      let awaitPageShow2 = BrowserTestUtils.waitForContentEvent(
        browser,
        "pageshow"
      );
      await SpecialPowers.spawn(browser, [], async function () {
        content.history.go(4);
      });
      await awaitPageShow2;
      ok(true, "Didn't time out.");
    }
  );
});
