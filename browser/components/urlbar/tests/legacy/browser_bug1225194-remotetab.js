const TEST_PATH = getRootDirectory(gTestPath)
  .replace("chrome://mochitests/content", "http://example.org");
const TEST_URL = `${TEST_PATH}dummy_page.html`;

add_task(async function test_remotetab_opens() {
  await BrowserTestUtils.withNewTab({url: "about:robots", gBrowser}, async function() {
    // Set the urlbar to include the moz-action
    gURLBar.value = "moz-action:remotetab," + JSON.stringify({ url: TEST_URL });
    // Focus the urlbar so we can press enter
    gURLBar.focus();

    // The URL is going to open in the current tab as it is currently about:blank
    let promiseTabLoaded = BrowserTestUtils.browserLoaded(gBrowser.selectedBrowser);
    EventUtils.synthesizeKey("KEY_Enter");
    await promiseTabLoaded;

    Assert.equal(gBrowser.selectedTab.linkedBrowser.currentURI.spec, TEST_URL, "correct URL loaded");
  });
});
