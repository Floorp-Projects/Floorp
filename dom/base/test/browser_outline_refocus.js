const URL = `data:text/html,<a target="_blank" href="http://example.com">Click me</a>`;

async function test_browser_outline_refocus(
  aMessage,
  aShouldFocusBeVisible,
  aOpenTabCallback
) {
  await BrowserTestUtils.withNewTab(URL, async function (browser) {
    let tab = gBrowser.getTabForBrowser(browser);
    let newTabPromise = BrowserTestUtils.waitForNewTab(gBrowser);

    await aOpenTabCallback(browser);

    info("waiting for new tab");
    let newTab = await newTabPromise;

    is(gBrowser.selectedTab, newTab, "Should've switched to the new tab");

    info("switching back");
    await BrowserTestUtils.switchTab(gBrowser, tab);

    info("checking focus");
    let [wasFocused, wasFocusVisible] = await SpecialPowers.spawn(
      browser,
      [],
      () => {
        let link = content.document.querySelector("a");
        return [link.matches(":focus"), link.matches(":focus-visible")];
      }
    );

    ok(wasFocused, "Link should be refocused");
    is(wasFocusVisible, aShouldFocusBeVisible, aMessage);

    info("closing tab");
    await BrowserTestUtils.removeTab(newTab);
  });
}

add_task(async function browser_outline_refocus_mouse() {
  await test_browser_outline_refocus(
    "Link shouldn't show outlines since it was originally focused by mouse",
    false,
    function (aBrowser) {
      info("clicking on link");
      return BrowserTestUtils.synthesizeMouseAtCenter("a", {}, aBrowser);
    }
  );
});

add_task(async function browser_outline_refocus_key() {
  await SpecialPowers.pushPrefEnv({
    set: [["accessibility.tabfocus", 7]],
  });

  await test_browser_outline_refocus(
    "Link should show outlines since it was originally focused by keyboard",
    true,
    function (aBrowser) {
      info("Navigating via keyboard");
      EventUtils.sendKey("tab");
      EventUtils.sendKey("return");
    }
  );
});
