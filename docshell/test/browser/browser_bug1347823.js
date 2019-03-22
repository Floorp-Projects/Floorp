/**
 * Test that session history's expiration tracker would remove bfcache on
 * expiration.
 */

// With bfcache not expired.
add_task(async function testValidCache() {
  // Make an unrealistic large timeout.
  await SpecialPowers.pushPrefEnv({
    set: [["browser.sessionhistory.contentViewerTimeout", 86400]],
  });

  await BrowserTestUtils.withNewTab(
    {gBrowser, url: "data:text/html;charset=utf-8,page1"},
    async function(browser) {
      // Make a simple modification for bfcache testing.
      await ContentTask.spawn(browser, null, () => {
        content.document.body.textContent = "modified";
      });

      // Load a random page.
      BrowserTestUtils.loadURI(browser, "data:text/html;charset=utf-8,page2");
      await BrowserTestUtils.browserLoaded(browser);

      // Go back and verify text content.
      let awaitPageShow = BrowserTestUtils.waitForContentEvent(browser, "pageshow");
      browser.goBack();
      await awaitPageShow;
      await ContentTask.spawn(browser, null, () => {
        is(content.document.body.textContent, "modified");
      });
    });
});

// With bfcache expired.
add_task(async function testExpiredCache() {
  // Make bfcache timeout in 1 sec.
  await SpecialPowers.pushPrefEnv({
    set: [["browser.sessionhistory.contentViewerTimeout", 1]],
  });

  await BrowserTestUtils.withNewTab(
    {gBrowser, url: "data:text/html;charset=utf-8,page1"},
    async function(browser) {
      // Make a simple modification for bfcache testing.
      await ContentTask.spawn(browser, null, () => {
        content.document.body.textContent = "modified";
      });

      // Load a random page.
      BrowserTestUtils.loadURI(browser, "data:text/html;charset=utf-8,page2");
      await BrowserTestUtils.browserLoaded(browser);

      // Wait for 3 times of expiration timeout, hopefully it's evicted...
      await ContentTask.spawn(browser, null, () => {
        return new Promise(resolve => {
          content.setTimeout(resolve, 3000);
        });
      });

      // Go back and verify text content.
      let awaitPageShow = BrowserTestUtils.waitForContentEvent(browser, "pageshow");
      browser.goBack();
      await awaitPageShow;
      await ContentTask.spawn(browser, null, () => {
        is(content.document.body.textContent, "page1");
      });
    });
});
