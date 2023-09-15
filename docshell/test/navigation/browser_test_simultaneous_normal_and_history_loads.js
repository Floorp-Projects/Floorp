/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

add_task(async function test_normal_and_history_loads() {
  // This test is for the case when session history lives in the parent process.
  // BFCache is disabled to get more asynchronousness.
  await SpecialPowers.pushPrefEnv({
    set: [["fission.bfcacheInParent", false]],
  });

  let testPage =
    getRootDirectory(gTestPath).replace(
      "chrome://mochitests/content",
      // eslint-disable-next-line @microsoft/sdl/no-insecure-url
      "http://example.com"
    ) + "blank.html";
  await BrowserTestUtils.withNewTab(
    { gBrowser, url: testPage },
    async function (browser) {
      for (let i = 0; i < 2; ++i) {
        BrowserTestUtils.startLoadingURIString(browser, testPage + "?" + i);
        await BrowserTestUtils.browserLoaded(browser);
      }

      let sh = browser.browsingContext.sessionHistory;
      is(sh.count, 3, "Should have 3 entries in the session history.");
      is(sh.index, 2, "index should be 2");
      is(sh.requestedIndex, -1, "requestedIndex should be -1");

      // The following part is racy by definition. It is testing that
      // eventually requestedIndex should become -1 again.
      browser.browsingContext.goToIndex(1);
      let historyLoad = BrowserTestUtils.browserLoaded(browser);
      /* eslint-disable mozilla/no-arbitrary-setTimeout */
      await new Promise(r => {
        setTimeout(r, 10);
      });
      browser.browsingContext.loadURI(
        Services.io.newURI(testPage + "?newload"),
        {
          triggeringPrincipal: browser.nodePrincipal,
        }
      );
      let newLoad = BrowserTestUtils.browserLoaded(browser);
      // Note, the loads are racy.
      await historyLoad;
      await newLoad;
      is(sh.requestedIndex, -1, "requestedIndex should be -1");

      browser.browsingContext.goBack();
      await BrowserTestUtils.browserLoaded(browser);
      is(sh.requestedIndex, -1, "requestedIndex should be -1");
    }
  );
});
