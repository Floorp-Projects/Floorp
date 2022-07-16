"use strict";

/**
 * Disable keyword.enabled (so no keyword search), and check that when
 * you type in "example" and hit enter, the browser shows an error page.
 */
add_task(async function() {
  await SpecialPowers.pushPrefEnv({
    set: [["keyword.enabled", false]],
  });
  await BrowserTestUtils.withNewTab(
    { gBrowser, url: "about:blank" },
    async function(browser) {
      gURLBar.value = "example";
      gURLBar.select();
      const loadPromise = BrowserTestUtils.waitForErrorPage(browser);
      EventUtils.sendKey("return");
      await loadPromise;
      ok(true, "error page is loaded correctly");
    }
  );
});

/**
 * Disable keyword.enabled (so no keyword search) and enable fixup.alternate, and check
 * that when you type in "example" and hit enter, the browser loads and the URL bar
 * is updated accordingly.
 */
add_task(async function() {
  await SpecialPowers.pushPrefEnv({
    set: [
      ["keyword.enabled", false],
      ["browser.fixup.alternate.enabled", true],
    ],
  });
  await BrowserTestUtils.withNewTab(
    { gBrowser, url: "about:blank" },
    async function(browser) {
      gURLBar.value = "example";
      gURLBar.select();
      const loadPromise = BrowserTestUtils.waitForDocLoadAndStopIt(
        "https://www.example.com/",
        gBrowser.selectedBrowser
      );

      EventUtils.sendKey("return");
      await loadPromise;
      ok(true, "https://www.example.com is loaded correctly");
    }
  );
});
