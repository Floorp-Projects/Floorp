add_task(function* test() {
  yield BrowserTestUtils.withNewTab("data:text/plain;charset=utf-8,1", function* (browser) {
    BrowserTestUtils.loadURI(browser, "data:text/plain;charset=utf-8,2");
    yield BrowserTestUtils.browserLoaded(browser);

    BrowserTestUtils.loadURI(browser, "data:text/plain;charset=utf-8,3");
    yield BrowserTestUtils.browserLoaded(browser);

    yield duplicate(0, "maintained the original index");
    yield BrowserTestUtils.removeTab(gBrowser.selectedTab);

    yield duplicate(-1, "went back");
    yield duplicate(1, "went forward");
    yield BrowserTestUtils.removeTab(gBrowser.selectedTab);
    yield BrowserTestUtils.removeTab(gBrowser.selectedTab);
  });
});

function promiseGetIndex(browser) {
  return ContentTask.spawn(browser, null, function() {
    let shistory = docShell.QueryInterface(Ci.nsIInterfaceRequestor)
                           .getInterface(Ci.nsISHistory);
    return shistory.index;
  });
}

let duplicate = Task.async(function* (delta, msg, cb) {
  var startIndex = yield promiseGetIndex(gBrowser.selectedBrowser);

  duplicateTabIn(gBrowser.selectedTab, "tab", delta);

  let tab = gBrowser.selectedTab;
  yield BrowserTestUtils.waitForEvent(tab, "SSTabRestored");

  let endIndex = yield promiseGetIndex(gBrowser.selectedBrowser);
  is(endIndex, startIndex + delta, msg);
});
