/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */
/* eslint-disable mozilla/no-arbitrary-setTimeout */

var tabState = {
  entries: [
    {
      url: "about:robots",
      triggeringPrincipal_base64,
      children: [{ url: "about:mozilla", triggeringPrincipal_base64 }],
    },
  ],
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
    let entry;
    if (!Services.appinfo.sessionHistoryInParent) {
      let sessionHistory = browser.sessionHistory;
      entry = sessionHistory.legacySHistory.getEntryAtIndex(0);
    } else {
      let sessionHistory = browser.browsingContext.sessionHistory;
      entry = sessionHistory.getEntryAtIndex(0);
    }

    whenChildCount(entry, 1, function() {
      whenChildCount(entry, 2, function() {
        promiseBrowserLoaded(browser)
          .then(() => {
            return TabStateFlusher.flush(browser);
          })
          .then(() => {
            let { entries } = JSON.parse(ss.getTabState(tab));
            is(entries.length, 1, "tab has one history entry");
            ok(!entries[0].children, "history entry has no subframes");

            // Make sure that we reset the state.
            let blankState = {
              windows: [
                {
                  tabs: [
                    {
                      entries: [
                        { url: "about:blank", triggeringPrincipal_base64 },
                      ],
                    },
                  ],
                },
              ],
            };
            waitForBrowserState(blankState, finish);
          });

        // Force reload the browser to deprecate the subframes.
        browser.reloadWithFlags(Ci.nsIWebNavigation.LOAD_FLAGS_BYPASS_CACHE);
      });

      // Create a dynamic subframe.
      let doc = browser.contentDocument;
      let iframe = doc.createElement("iframe");
      doc.body.appendChild(iframe);
      iframe.setAttribute("src", "about:mozilla");
    });
  });
}

function whenChildCount(aEntry, aChildCount, aCallback) {
  if (aEntry.childCount == aChildCount) {
    aCallback();
  } else {
    setTimeout(() => whenChildCount(aEntry, aChildCount, aCallback), 100);
  }
}
