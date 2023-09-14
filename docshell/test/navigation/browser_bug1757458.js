/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

add_task(async function () {
  let testPage =
    getRootDirectory(gTestPath).replace(
      "chrome://mochitests/content",
      "view-source:http://example.com"
    ) + "redirect_to_blank.sjs";

  let testPage2 = "data:text/html,<div>Second page</div>";
  await BrowserTestUtils.withNewTab(
    { gBrowser, url: testPage },
    async function (browser) {
      await ContentTask.spawn(browser, [], async () => {
        Assert.ok(
          content.document.getElementById("viewsource").localName == "body",
          "view-source document's body should have id='viewsource'."
        );
        content.document
          .getElementById("viewsource")
          .setAttribute("onunload", "/* disable bfcache*/");
      });

      BrowserTestUtils.startLoadingURIString(browser, testPage2);
      await BrowserTestUtils.browserLoaded(browser);

      let pageShownPromise = BrowserTestUtils.waitForContentEvent(
        browser,
        "pageshow",
        true
      );
      browser.browsingContext.goBack();
      await pageShownPromise;

      await ContentTask.spawn(browser, [], async () => {
        Assert.ok(
          content.document.getElementById("viewsource").localName == "body",
          "view-source document's body should have id='viewsource'."
        );
      });
    }
  );
});
