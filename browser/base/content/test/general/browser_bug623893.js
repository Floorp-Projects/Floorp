add_task(async function test() {
  await BrowserTestUtils.withNewTab(
    "data:text/plain;charset=utf-8,1",
    async function (browser) {
      BrowserTestUtils.startLoadingURIString(
        browser,
        "data:text/plain;charset=utf-8,2"
      );
      await BrowserTestUtils.browserLoaded(browser);

      BrowserTestUtils.startLoadingURIString(
        browser,
        "data:text/plain;charset=utf-8,3"
      );
      await BrowserTestUtils.browserLoaded(browser);

      await duplicate(0, "maintained the original index");
      BrowserTestUtils.removeTab(gBrowser.selectedTab);

      await duplicate(-1, "went back");
      await duplicate(1, "went forward");
      BrowserTestUtils.removeTab(gBrowser.selectedTab);
      BrowserTestUtils.removeTab(gBrowser.selectedTab);
    }
  );
});

async function promiseGetIndex(browser) {
  if (!SpecialPowers.Services.appinfo.sessionHistoryInParent) {
    return SpecialPowers.spawn(browser, [], function () {
      let shistory =
        docShell.browsingContext.childSessionHistory.legacySHistory;
      return shistory.index;
    });
  }

  let shistory = browser.browsingContext.sessionHistory;
  return shistory.index;
}

let duplicate = async function (delta, msg, cb) {
  var startIndex = await promiseGetIndex(gBrowser.selectedBrowser);

  duplicateTabIn(gBrowser.selectedTab, "tab", delta);

  await BrowserTestUtils.waitForEvent(gBrowser.tabContainer, "SSTabRestored");

  let endIndex = await promiseGetIndex(gBrowser.selectedBrowser);
  is(endIndex, startIndex + delta, msg);
};
