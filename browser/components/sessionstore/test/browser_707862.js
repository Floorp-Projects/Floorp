/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

let tabState = {
  entries: [{url: "about:home", children: [{url: "about:mozilla"}]}]
};

function test() {
  waitForExplicitFinish();

  let tab = gBrowser.addTab("about:blank");
  registerCleanupFunction(function () gBrowser.removeTab(tab));

  let browser = tab.linkedBrowser;

  whenBrowserLoaded(browser, function () {
    ss.setTabState(tab, JSON.stringify(tabState));

    let sessionHistory = browser.sessionHistory;
    let entry = sessionHistory.getEntryAtIndex(0, false);

    whenChildCount(entry, 1, function () {
      whenChildCount(entry, 2, function () {
        whenBrowserLoaded(browser, function () {
          let sessionHistory = browser.sessionHistory;
          let entry = sessionHistory.getEntryAtIndex(0, false);

          whenChildCount(entry, 0, finish);
        });

        // reload the browser to deprecate the subframes
        browser.reload();
      });

      // create a dynamic subframe
      let doc = browser.contentDocument;
      let iframe = doc.createElement("iframe");
      iframe.setAttribute("src", "about:mozilla");
      doc.body.appendChild(iframe);
    });
  });
}

function whenBrowserLoaded(aBrowser, aCallback) {
  aBrowser.addEventListener("load", function onLoad() {
    aBrowser.removeEventListener("load", onLoad, true);
    executeSoon(aCallback);
  }, true);
}

function whenChildCount(aEntry, aChildCount, aCallback) {
  if (aEntry.childCount == aChildCount)
    aCallback();
  else
    executeSoon(function () whenChildCount(aEntry, aChildCount, aCallback));
}
