/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */

const TEST_HTTP = httpURL("dummy_page.html");

async function doNavigateHome(expectedURL, expectFocused, waitForCondition) {
  await BrowserTestUtils.withNewTab(TEST_HTTP, async function(browser) {
    info("Tab ready");

    document.getElementById("home-button").click();
    await BrowserTestUtils.browserLoaded(browser, false, HomePage.get());

    if (waitForCondition) {
      await waitForCondition(browser);
    }

    is(gURLBar.value, expectedURL, "URL bar set correctly");
    is(
      gURLBar.focused,
      expectFocused,
      "URL bar should" + (expectFocused ? "" : " not") + " be focused"
    );
  });
}

// Test for Bug 1634272
add_task(async function testNavHomeDefault() {
  await doNavigateHome("", true);
});

add_task(async function testNavHomeHTTP() {
  await SpecialPowers.pushPrefEnv({
    set: [["browser.startup.homepage", "http://example.com/"]],
  });

  // We don't focus the URL bar when the home page is a website.
  await doNavigateHome("example.com", false);

  await SpecialPowers.popPrefEnv();
});

// Test for Bug 1643578
add_task(async function testNavHomeExtension() {
  // Mostly copied from browser_ext_chrome_settings_overrides_home.js.
  let extension = ExtensionTestUtils.loadExtension({
    manifest: {
      applications: {
        gecko: { id: "extension@mochi.test" },
      },
      name: "Extension",
      chrome_settings_overrides: { homepage: "ext.html" },
      // We'll only focus the URL bar on a moz-extension:// home page if we're
      // using the same custom URL for the newtab page.
      chrome_url_overrides: { newtab: "ext.html" },
    },
    files: { "ext.html": "<h1>1</h1>" },
    useAddonManager: "temporary",
  });

  await extension.startup();

  // This is a little weird. On the initial home navigation, we ask the user
  // for confirmation that the new extension should override the home page. This
  // confirmation dialog is focused. Forcing an extra home navigation emulates
  // clicking "keep changes" on that dialog, and avoids adding more complexity
  // here.

  // Ensure that we only proceed after the confirmation dialog has shown up.
  await doNavigateHome("", false, browser =>
    TestUtils.waitForCondition(() =>
      browser.ownerDocument.getElementById("extension-homepage-notification")
    )
  );
  await doNavigateHome("", true);

  await extension.unload();
});
