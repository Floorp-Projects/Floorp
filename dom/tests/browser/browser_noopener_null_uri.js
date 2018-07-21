add_task(async function browserNoopenerNullUri() {
  await BrowserTestUtils.withNewTab({gBrowser}, async function(aBrowser) {
    let numTabs = gBrowser.tabs.length;
    await ContentTask.spawn(aBrowser, null, async () => {
      ok(!content.window.open(undefined, undefined, 'noopener'),
         "window.open should return null");
    });
    await TestUtils.waitForCondition(() => gBrowser.tabs.length == numTabs + 1);
    // We successfully opened a tab in content process!
  });
  // We only have to close the tab we opened earlier
  await BrowserTestUtils.removeTab(gBrowser.tabs[1]);
});
