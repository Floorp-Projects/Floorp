add_task(async function browserNoopenerNullUri() {
  await BrowserTestUtils.withNewTab({gBrowser}, async function(aBrowser) {
    let waitFor = BrowserTestUtils.waitForNewWindow();
    await ContentTask.spawn(aBrowser, null, async () => {
      ok(!content.window.open(undefined, undefined, 'noopener'),
         "window.open should return null");
    });
    let win = await waitFor;
    ok(win, "We successfully opened a new window in the content process!");
    await BrowserTestUtils.closeWindow(win);
  });
});
