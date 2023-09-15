/**
 * Test that session history's expiration tracker would remove bfcache on
 * expiration.
 */

// With bfcache not expired.
add_task(async function testValidCache() {
  // Make an unrealistic large timeout.
  await SpecialPowers.pushPrefEnv({
    set: [
      ["browser.sessionhistory.contentViewerTimeout", 86400],
      // If Fission is disabled, the pref is no-op.
      ["fission.bfcacheInParent", true],
    ],
  });

  await BrowserTestUtils.withNewTab(
    { gBrowser, url: "data:text/html;charset=utf-8,pageA1" },
    async function (browser) {
      // Make a simple modification for bfcache testing.
      await SpecialPowers.spawn(browser, [], () => {
        content.document.body.textContent = "modified";
      });

      // Load a random page.
      BrowserTestUtils.startLoadingURIString(
        browser,
        "data:text/html;charset=utf-8,pageA2"
      );
      await BrowserTestUtils.browserLoaded(browser);

      // Go back and verify text content.
      let awaitPageShow = BrowserTestUtils.waitForContentEvent(
        browser,
        "pageshow"
      );
      browser.goBack();
      await awaitPageShow;
      await SpecialPowers.spawn(browser, [], () => {
        is(content.document.body.textContent, "modified");
      });
    }
  );
});

// With bfcache expired.
add_task(async function testExpiredCache() {
  // Make bfcache timeout in 1 sec.
  await SpecialPowers.pushPrefEnv({
    set: [
      ["browser.sessionhistory.contentViewerTimeout", 1],
      // If Fission is disabled, the pref is no-op.
      ["fission.bfcacheInParent", true],
    ],
  });

  await BrowserTestUtils.withNewTab(
    { gBrowser, url: "data:text/html;charset=utf-8,pageB1" },
    async function (browser) {
      // Make a simple modification for bfcache testing.
      await SpecialPowers.spawn(browser, [], () => {
        content.document.body.textContent = "modified";
      });

      // Load a random page.
      BrowserTestUtils.startLoadingURIString(
        browser,
        "data:text/html;charset=utf-8,pageB2"
      );
      await BrowserTestUtils.browserLoaded(browser);

      // Wait for 3 times of expiration timeout, hopefully it's evicted...
      await SpecialPowers.spawn(browser, [], () => {
        return new Promise(resolve => {
          content.setTimeout(resolve, 5000);
        });
      });

      // Go back and verify text content.
      let awaitPageShow = BrowserTestUtils.waitForContentEvent(
        browser,
        "pageshow"
      );
      browser.goBack();
      await awaitPageShow;
      await SpecialPowers.spawn(browser, [], () => {
        is(content.document.body.textContent, "pageB1");
      });
    }
  );
});
