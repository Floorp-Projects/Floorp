/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const TEST_PAGE_1 =
  "http://mochi.test:8888/browser/docshell/test/browser/dummy_page.html";

const TEST_PAGE_2 =
  // eslint-disable-next-line @microsoft/sdl/no-insecure-url
  "http://example.com/browser/docshell/test/browser/dummy_page.html";

add_task(async function test() {
  await BrowserTestUtils.withNewTab(TEST_PAGE_1, async function (browser) {
    let loaded = BrowserTestUtils.browserLoaded(browser, false, TEST_PAGE_2);
    await SpecialPowers.spawn(browser, [], () => {
      content.addEventListener("unload", e => e.currentTarget.stop(), true);
    });
    BrowserTestUtils.startLoadingURIString(browser, TEST_PAGE_2);
    await loaded;
    ok(true, "Page loaded successfully");
  });
});
