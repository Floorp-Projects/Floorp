add_task(async function() {
  let url =
    getRootDirectory(gTestPath).replace(
      "chrome://mochitests/content/",
      "http://example.com/"
    ) + "file_bug1206879.html";
  let tab = await BrowserTestUtils.openNewForegroundTab(gBrowser, url, true);

  let numLocationChanges = 0;

  let listener = {
    onLocationChange(browser, wp, request, uri, flags) {
      if (browser != tab.linkedBrowser) {
        return;
      }
      info("onLocationChange: " + uri.spec);
      numLocationChanges++;
      this.resolve();
    },
  };
  let locationPromise = new Promise((resolve, reject) => {
    listener.resolve = resolve;
    gBrowser.addTabsProgressListener(listener);
  });
  await ContentTask.spawn(tab.linkedBrowser, {}, function() {
    content.frames[0].history.pushState(null, null, "foo");
  });

  await locationPromise;

  gBrowser.removeTab(tab);
  gBrowser.removeTabsProgressListener(listener);
  is(
    numLocationChanges,
    1,
    "pushState with a different URI should cause a LocationChange event."
  );
});
