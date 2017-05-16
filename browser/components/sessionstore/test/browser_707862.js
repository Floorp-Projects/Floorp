/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

var tabState = {
  entries: [{
    url: "about:robots",
    triggeringPrincipal_base64,
    children: [{url: "about:mozilla", triggeringPrincipal_base64, }]}]
};

function test() {
  waitForExplicitFinish();
  requestLongerTimeout(2);

  Services.prefs.setIntPref("browser.sessionstore.interval", 4000);
  registerCleanupFunction(function() {
    Services.prefs.clearUserPref("browser.sessionstore.interval");
  });

  let tab = BrowserTestUtils.addTab(gBrowser, "about:blank");

  let browser = tab.linkedBrowser;

  promiseTabState(tab, tabState).then(() => {
    let sessionHistory = browser.sessionHistory;
    let entry = sessionHistory.getEntryAtIndex(0, false);
    entry.QueryInterface(Ci.nsISHContainer);

    whenChildCount(entry, 1, function() {
      whenChildCount(entry, 2, function() {
        promiseBrowserLoaded(browser).then(() => {
          let newSessionHistory = browser.sessionHistory;
          let newEntry = newSessionHistory.getEntryAtIndex(0, false);

          whenChildCount(newEntry, 0, function() {
            // Make sure that we reset the state.
            let blankState = { windows: [{ tabs: [{ entries: [{ url: "about:blank",
                                                                triggeringPrincipal_base64 }] }]}]};
            waitForBrowserState(blankState, finish);
          });
        });

        // reload the browser to deprecate the subframes
        browser.reload();
      });

      // create a dynamic subframe
      let doc = browser.contentDocument;
      let iframe = doc.createElement("iframe");
      doc.body.appendChild(iframe);
      iframe.setAttribute("src", "about:mozilla");
    });
  });

  // This test relies on the test timing out in order to indicate failure so
  // let's add a dummy pass.
  ok(true, "Each test requires at least one pass, fail or todo so here is a pass.");
}

function whenChildCount(aEntry, aChildCount, aCallback) {
  if (aEntry.childCount == aChildCount)
    aCallback();
  else
    setTimeout(() => whenChildCount(aEntry, aChildCount, aCallback), 100);
}
