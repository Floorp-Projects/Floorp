/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */

const TEST_HTTP = httpURL("dummy_page.html");

async function doNavigateHome(isHome, expectedURL, expectFocused) {
  await BrowserTestUtils.withNewTab(TEST_HTTP, async function(browser) {
    info("Tab ready");

    document.getElementById("home-button").click();
    await BrowserTestUtils.browserLoaded(browser, false, isHome);
    is(gURLBar.focused, expectFocused, "URL bar should" 
      + (expectFocused ? "" : " not") + " be focused");
    is(gURLBar.value, expectedURL, "URL bar set correctly");
  });
}

// Test for Bug 1634272
add_task(async function testNavHomeDefault() {
  await doNavigateHome((url) => url === HomePage.get(), "",
    true);
});


add_task(async function testNavHomePage() {
  await SpecialPowers.pushPrefEnv({
    set: [["browser.startup.homepage", "http://example.com"]]
  });

  // We don't focus the URL bar when the home page is a website.
  await doNavigateHome((url) => url === "http://example.com/",
      "example.com", false);

  await SpecialPowers.popPrefEnv();
});

