let wrapper = {};
Cu.import("resource:///modules/sessionstore/TabStateCache.jsm", wrapper);
let {TabStateCache} = wrapper;

let ss = Cc["@mozilla.org/browser/sessionstore;1"].getService(Ci.nsISessionStore);

/*
 * Bug 930202.
 *
 * This test initiates session restoration part way through async
 * session collection. We want to ensure that the session gets
 * restored correctly, and that the collection doesn't poison the
 * TabStateCache with inconsistent data.
 *
 * Between starting the collection and restoring, we cycle through the
 * event loop a few times. To make the test as general as possible, we
 * run the test several times, with a different number of cycles in
 * between each time.
 */

function test() {
  waitForExplicitFinish();

  // We cycle through the event loop 1, 3, 5, or 7 times.
  let runs = [1, 3, 5, 7];

  function runOneTest() {
    if (runs.length == 0) {
      finish();
      return;
    }

    let pauses = runs.shift();
    testPauses(pauses, runOneTest);
  }

  runOneTest();
}

function testPauses(numPauses, done) {
  let tab = gBrowser.addTab("about:robots");

  // Force saving at the beginning to ensure we start in a clean state.
  forceSaveState().then(function() {
    TabStateCache.clear();

    // Trigger a new async collection.
    const PREF = "browser.sessionstore.interval";
    Services.prefs.setIntPref(PREF, 1000);
    Services.prefs.setIntPref(PREF, 0);

    let tabState = {
      entries: [{url: "http://example.com"}]
    };

    // Cycle through the event loop numPauses times.
    function go(n) {
      if (n < numPauses) {
        executeSoon(() => go(n+1));
        return;
      }

      // Trigger tab restoration.
      ss.setTabState(tab, JSON.stringify(tabState));

      // Once we finish collection, ensure that we ended up with the right data.
      waitForTopic("sessionstore-state-write", 1000, function () {
        let state = ss.getTabState(tab);
        ok(state.indexOf("example.com") != -1,
           "getTabState returns correct URL after " + numPauses + " pauses");

        gBrowser.removeTab(tab);
        done();
      });
    }

    go(0);
  }, Cu.reportError);
}
