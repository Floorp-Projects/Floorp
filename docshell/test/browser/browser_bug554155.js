add_task(async function test() {
  await BrowserTestUtils.withNewTab({ gBrowser, url: "http://example.com" }, async function(browser) {
    let numLocationChanges = 0;

    let listener = {
      onLocationChange(browser, webProgress, request, uri, flags) {
        info("location change: " + (uri && uri.spec));
        numLocationChanges++;
      },
    };

    gBrowser.addTabsProgressListener(listener);

    await ContentTask.spawn(browser, null, function() {
      // pushState to a new URL (http://example.com/foo").  This should trigger
      // exactly one LocationChange event.
      content.history.pushState(null, null, "foo");
    });

    await Promise.resolve();

    gBrowser.removeTabsProgressListener(listener);
    is(numLocationChanges, 1,
       "pushState should cause exactly one LocationChange event.");
  });
});
