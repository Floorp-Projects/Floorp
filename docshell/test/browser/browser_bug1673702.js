const DUMMY =
  // eslint-disable-next-line @microsoft/sdl/no-insecure-url
  "http://example.org/browser/docshell/test/browser/dummy_page.html";
const JSON =
  // eslint-disable-next-line @microsoft/sdl/no-insecure-url
  "http://example.com/browser/docshell/test/browser/file_bug1673702.json";

add_task(async function test_backAndReload() {
  await BrowserTestUtils.withNewTab(
    { gBrowser, url: DUMMY },
    async function (browser) {
      info("Start JSON load.");
      BrowserTestUtils.startLoadingURIString(browser, JSON);
      await BrowserTestUtils.waitForLocationChange(gBrowser, JSON);

      info("JSON load has started, go back.");
      browser.goBack();
      await BrowserTestUtils.browserStopped(browser);

      info("Reload.");
      BrowserReload();
      await BrowserTestUtils.waitForLocationChange(gBrowser);

      is(browser.documentURI.spec, DUMMY);
    }
  );
});
