add_task(function* test() {
  yield BrowserTestUtils.withNewTab({ gBrowser, url: "http://example.com" }, function* (browser) {
    let numLocationChanges = 0;

    let listener = {
      onLocationChange: function(browser, webProgress, request, uri, flags) {
        info("location change: " + (uri && uri.spec));
        numLocationChanges++;
      }
    };

    gBrowser.addTabsProgressListener(listener);

    yield ContentTask.spawn(browser, null, function() {
      // pushState to a new URL (http://example.com/foo").  This should trigger
      // exactly one LocationChange event.
      content.history.pushState(null, null, "foo");
    });

    yield Promise.resolve();

    gBrowser.removeTabsProgressListener(listener);
    is(numLocationChanges, 1,
       "pushState should cause exactly one LocationChange event.");
  });
});
