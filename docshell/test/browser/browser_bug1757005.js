/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

add_task(async function () {
  // (1) Load one page with bfcache disabled and another one with bfcache enabled.
  // (2) Check that BrowsingContext.getCurrentTopByBrowserId(browserId) returns
  //     the expected browsing context both in the parent process and in the child process.
  // (3) Go back and then forward
  // (4) Run the same checks as in step 2 again.

  let url1 = "data:text/html,<body onunload='/* disable bfcache */'>";
  let url2 = "data:text/html,page2";
  await BrowserTestUtils.withNewTab(
    {
      gBrowser,
      url: url1,
    },
    async function (browser) {
      info("Initial load");

      let loaded = BrowserTestUtils.browserLoaded(browser);
      BrowserTestUtils.startLoadingURIString(browser, url2);
      await loaded;
      info("Second page loaded");

      let browserId = browser.browserId;
      ok(!!browser.browsingContext, "Should have a BrowsingContext. (1)");
      is(
        BrowsingContext.getCurrentTopByBrowserId(browserId),
        browser.browsingContext,
        "Should get the correct browsingContext(1)"
      );

      await ContentTask.spawn(browser, browserId, async function (browserId) {
        Assert.ok(
          BrowsingContext.getCurrentTopByBrowserId(browserId) ==
            docShell.browsingContext
        );
        Assert.ok(docShell.browsingContext.browserId == browserId);
      });

      let awaitPageShow = BrowserTestUtils.waitForContentEvent(
        browser,
        "pageshow"
      );
      browser.goBack();
      await awaitPageShow;
      info("Back");

      awaitPageShow = BrowserTestUtils.waitForContentEvent(browser, "pageshow");
      browser.goForward();
      await awaitPageShow;
      info("Forward");

      ok(!!browser.browsingContext, "Should have a BrowsingContext. (2)");
      is(
        BrowsingContext.getCurrentTopByBrowserId(browserId),
        browser.browsingContext,
        "Should get the correct BrowsingContext. (2)"
      );

      await ContentTask.spawn(browser, browserId, async function (browserId) {
        Assert.ok(
          BrowsingContext.getCurrentTopByBrowserId(browserId) ==
            docShell.browsingContext
        );
        Assert.ok(docShell.browsingContext.browserId == browserId);
      });
    }
  );
});
