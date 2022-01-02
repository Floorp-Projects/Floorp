/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */

const TEST_HTTP = httpURL("dummy_page.html");

// Test for Bug 1634272
add_task(async function() {
  await BrowserTestUtils.withNewTab(TEST_HTTP, async function(browser) {
    info("Tab ready");

    CustomizableUI.addWidgetToArea("home-button", "nav-bar");
    registerCleanupFunction(() =>
      CustomizableUI.removeWidgetFromArea("home-button")
    );

    document.getElementById("home-button").click();
    await BrowserTestUtils.browserLoaded(browser, false, HomePage.get());
    is(gURLBar.value, "", "URL bar should be empty");
    ok(gURLBar.focused, "URL bar should be focused");
  });
});
