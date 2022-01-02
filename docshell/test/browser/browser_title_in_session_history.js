/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

add_task(async function test() {
  const TEST_PAGE =
    getRootDirectory(gTestPath).replace(
      "chrome://mochitests/content",
      "https://example.com"
    ) + "dummy_page.html";
  await BrowserTestUtils.withNewTab(TEST_PAGE, async browser => {
    let titles = await ContentTask.spawn(browser, null, () => {
      return new Promise(resolve => {
        let titles = [];
        content.document.body.innerHTML = "<div id='foo'>foo</div>";
        content.document.title = "Initial";
        content.history.pushState("1", "1", "1");
        content.document.title = "1";
        content.history.pushState("2", "2", "2");
        content.document.title = "2";
        content.location.hash = "hash";
        content.document.title = "3-hash";
        content.addEventListener(
          "popstate",
          () => {
            content.addEventListener(
              "popstate",
              () => {
                titles.push(content.document.title);
                resolve(titles);
              },
              { once: true }
            );

            titles.push(content.document.title);
            // Test going forward a few steps.
            content.history.go(2);
          },
          { once: true }
        );
        // Test going back a few steps.
        content.history.go(-3);
      });
    });
    is(
      titles[0],
      "3-hash",
      "Document.title should have the value to which it was last time set."
    );
    is(
      titles[1],
      "3-hash",
      "Document.title should have the value to which it was last time set."
    );
    let sh = browser.browsingContext.sessionHistory;
    let count = sh.count;
    is(sh.getEntryAtIndex(count - 1).title, "3-hash");
    is(sh.getEntryAtIndex(count - 2).title, "2");
    is(sh.getEntryAtIndex(count - 3).title, "1");
    is(sh.getEntryAtIndex(count - 4).title, "Initial");
  });
});
