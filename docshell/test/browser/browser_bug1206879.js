add_task(function*() {
  let url = getRootDirectory(gTestPath) + "file_bug1206879.html";
  let tab = yield BrowserTestUtils.openNewForegroundTab(gBrowser, url, true);

  let numLocationChanges = 0;

  let listener = {
    onLocationChange: function(browser, wp, request, uri, flags) {
      if (browser != tab.linkedBrowser) {
        return;
      }
      info("onLocationChange: " + uri.spec);
      numLocationChanges++;
      this.resolve();
    }
  };
  let locationPromise = new Promise((resolve, reject) => {
    listener.resolve = resolve;
    gBrowser.addTabsProgressListener(listener);
  });
  yield ContentTask.spawn(tab.linkedBrowser, {}, function() {
    content.frames[0].history.pushState(null, null, "foo");
  });

  yield locationPromise;

  gBrowser.removeTab(tab);
  gBrowser.removeTabsProgressListener(listener);
  is(numLocationChanges, 1,
     "pushState with a different URI should cause a LocationChange event.");
});
