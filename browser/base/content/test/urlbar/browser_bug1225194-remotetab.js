add_task(async function test_remotetab_opens() {
  const url = "http://example.org/browser/browser/base/content/test/urlbar/dummy_page.html";
  await BrowserTestUtils.withNewTab({url: "about:robots", gBrowser}, async function() {
    // Set the urlbar to include the moz-action
    gURLBar.value = "moz-action:remotetab," + JSON.stringify({ url });
    // Focus the urlbar so we can press enter
    gURLBar.focus();

    // The URL is going to open in the current tab as it is currently about:blank
    let promiseTabLoaded = BrowserTestUtils.browserLoaded(gBrowser.selectedBrowser);
    EventUtils.synthesizeKey("VK_RETURN", {});
    await promiseTabLoaded;

    Assert.equal(gBrowser.selectedTab.linkedBrowser.currentURI.spec, url, "correct URL loaded");
  });
});
