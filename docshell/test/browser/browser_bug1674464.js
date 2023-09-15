const DUMMY_1 =
  // eslint-disable-next-line @microsoft/sdl/no-insecure-url
  "http://example.org/browser/docshell/test/browser/dummy_page.html";
const DUMMY_2 =
  // eslint-disable-next-line @microsoft/sdl/no-insecure-url
  "http://example.com/browser/docshell/test/browser/dummy_page.html";

add_task(async function test_backAndReload() {
  await BrowserTestUtils.withNewTab(
    { gBrowser, url: DUMMY_1 },
    async function (browser) {
      await BrowserTestUtils.crashFrame(browser);

      info("Start second load.");
      BrowserTestUtils.startLoadingURIString(browser, DUMMY_2);
      await BrowserTestUtils.waitForLocationChange(gBrowser, DUMMY_2);

      browser.goBack();
      await BrowserTestUtils.waitForLocationChange(gBrowser);

      is(
        browser.browsingContext.childSessionHistory.index,
        0,
        "We should have gone back to the first page"
      );
      is(
        browser.browsingContext.childSessionHistory.count,
        2,
        "If a tab crashes after a load has finished we shouldn't have an entry for about:tabcrashed"
      );
      is(
        browser.documentURI.spec,
        DUMMY_1,
        "If a tab crashes after a load has finished we shouldn't have an entry for about:tabcrashed"
      );
    }
  );
});
