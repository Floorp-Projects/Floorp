"use strict";

const URL = "http://example.com/browser_switch_remoteness_";

function countHistoryEntries(browser) {
  return ContentTask.spawn(browser, null, function* () {
    let Ci = Components.interfaces;
    let webNavigation = docShell.QueryInterface(Ci.nsIWebNavigation);
    let history = webNavigation.sessionHistory.QueryInterface(Ci.nsISHistoryInternal);
    return history && history.count;
  });
}

add_task(function* () {
  // Add a new tab.
  let tab = gBrowser.addTab("about:blank");
  let browser = tab.linkedBrowser;
  yield promiseBrowserLoaded(browser);
  ok(browser.isRemoteBrowser, "browser is remote");

  // Get the maximum number of preceding entries to save.
  const MAX_BACK = Services.prefs.getIntPref("browser.sessionstore.max_serialize_back");
  ok(MAX_BACK > -1, "check that the default has a value that caps data");

  // Load more pages than we would save to disk on a clean shutdown.
  for (let i = 0; i < MAX_BACK + 2; i++) {
    browser.loadURI(URL + i);
    yield promiseBrowserLoaded(browser);
    ok(browser.isRemoteBrowser, "browser is still remote");
  }

  // Check we have the right number of shistory entries.
  let count = yield countHistoryEntries(browser);
  is(count, MAX_BACK + 2, "correct number of shistory entries");

  // Load a non-remote page.
  browser.loadURI("about:robots");
  yield promiseTabRestored(tab);
  ok(!browser.isRemoteBrowser, "browser is not remote anymore");

  // Check that we didn't lose any shistory entries.
  count = yield countHistoryEntries(browser);
  is(count, MAX_BACK + 3, "correct number of shistory entries");

  // Cleanup.
  gBrowser.removeTab(tab);
});

add_task(function* () {
  // Add a new non-remote tab.
  let tab = gBrowser.addTab("about:robots");
  let browser = tab.linkedBrowser;
  yield promiseBrowserLoaded(browser);
  ok(!browser.isRemoteBrowser, "browser is not remote");

  // Wait for the tab to change to remote before adding the progress listener
  tab.addEventListener("TabRemotenessChange", function listener() {
    tab.removeEventListener("TabRemotenessChange", listener);

    ContentTask.spawn(browser, URL, function*(url) {
      Cu.import("resource://gre/modules/XPCOMUtils.jsm");

      let wp = docShell.QueryInterface(Ci.nsIInterfaceRequestor)
                       .getInterface(Ci.nsIWebProgress);

      wp.addProgressListener({
        onStateChange: function(progress, request, stateFlags, status) {
          if (!(request instanceof Ci.nsIChannel))
            return;

          if (request.URI.spec == url) {
            request.cancel(Cr.NS_BINDING_ABORTED);
            wp.removeProgressListener(this);
          }
        },

        QueryInterface: XPCOMUtils.generateQI([
          Ci.nsIWebProgressListener,
          Ci.nsISupportsWeakReference
        ])
      }, Ci.nsIWebProgress.NOTIFY_ALL);
    });
  });

  // Load a new remote URI and when we see the load start cancel it
  browser.loadURI(URL);
  yield promiseTabRestored(tab);

  let count = yield countHistoryEntries(browser);
  is(count, 1, "Should only be the one history entry.");

  is(browser.currentURI.spec, "about:robots", "Should be back to the original URI");
  ok(!browser.isRemoteBrowser, "Should have gone back to a remote browser");

  // Cleanup.
  gBrowser.removeTab(tab);
});
