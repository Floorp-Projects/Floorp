/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

"use strict";

add_task(async function basic() {
  await BrowserTestUtils.withNewTab("http://example.com/", async () => {
    let queryContext = await clickDropmarker();
    is(queryContext.searchString, "",
       "Clicking the history dropmarker should initiate an empty search instead of searching for the loaded URL");
    is(gURLBar.value, "http://example.com/",
       "Clicking the history dropmarker should not change the input value");
    await UrlbarTestUtils.promisePopupClose(window,
      () => EventUtils.synthesizeKey("KEY_Escape"));
  });
});

add_task(async function proxyState() {
  // Open two tabs.
  await BrowserTestUtils.withNewTab("about:blank", async browser1 => {
    await BrowserTestUtils.withNewTab("http://example.com/", async browser2 => {
      // Click the dropmarker on the second tab and switch back to the first
      // tab.
      await clickDropmarker();
      await UrlbarTestUtils.promisePopupClose(window,
        () => EventUtils.synthesizeKey("KEY_Escape"));
      await BrowserTestUtils.switchTab(
        gBrowser,
        gBrowser.getTabForBrowser(browser1)
      );
      // Switch back to the second tab.
      await BrowserTestUtils.switchTab(
        gBrowser,
        gBrowser.getTabForBrowser(browser2)
      );
      // The proxy state should be valid.
      Assert.equal(gURLBar.getAttribute("pageproxystate"), "valid");
    });
  });
});

async function clickDropmarker() {
  await UrlbarTestUtils.promisePopupOpen(window, () => {
    let historyDropMarker =
      window.document.getAnonymousElementByAttribute(gURLBar.textbox, "anonid",
                                                     "historydropmarker");
    EventUtils.synthesizeMouseAtCenter(historyDropMarker, {}, window);
  });
  let queryContext = await gURLBar.lastQueryContextPromise;
  return queryContext;
}
