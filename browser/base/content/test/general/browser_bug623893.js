add_task(async function test() {
  await BrowserTestUtils.withNewTab("data:text/plain;charset=utf-8,1", async function(browser) {
    BrowserTestUtils.loadURI(browser, "data:text/plain;charset=utf-8,2");
    await BrowserTestUtils.browserLoaded(browser);

    BrowserTestUtils.loadURI(browser, "data:text/plain;charset=utf-8,3");
    await BrowserTestUtils.browserLoaded(browser);

    await duplicate(0, "maintained the original index");
    await BrowserTestUtils.removeTab(gBrowser.selectedTab);

    await duplicate(-1, "went back");
    await duplicate(1, "went forward");
    await BrowserTestUtils.removeTab(gBrowser.selectedTab);
    await BrowserTestUtils.removeTab(gBrowser.selectedTab);
  });
});

function promiseGetIndex(browser) {
  return ContentTask.spawn(browser, null, function() {
    let shistory = docShell.QueryInterface(Ci.nsIInterfaceRequestor)
                           .getInterface(Ci.nsISHistory);
    return shistory.index;
  });
}

let duplicate = async function(delta, msg, cb) {
  var startIndex = await promiseGetIndex(gBrowser.selectedBrowser);

  duplicateTabIn(gBrowser.selectedTab, "tab", delta);

  let tab = gBrowser.selectedTab;
  await BrowserTestUtils.waitForEvent(tab, "SSTabRestored");

  let endIndex = await promiseGetIndex(gBrowser.selectedBrowser);
  is(endIndex, startIndex + delta, msg);
};
