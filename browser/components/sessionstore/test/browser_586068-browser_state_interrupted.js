/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const PREF_RESTORE_ON_DEMAND = "browser.sessionstore.restore_on_demand";

add_task(function* test() {
  Services.prefs.setBoolPref(PREF_RESTORE_ON_DEMAND, false);
  registerCleanupFunction(function () {
    Services.prefs.clearUserPref(PREF_RESTORE_ON_DEMAND);
  });

  // The first state will be loaded using setBrowserState, followed by the 2nd
  // state also being loaded using setBrowserState, interrupting the first restore.
  let state1 = { windows: [
    {
      tabs: [
        { entries: [{ url: "http://example.org#1" }], extData: { "uniq": r() } },
        { entries: [{ url: "http://example.org#2" }], extData: { "uniq": r() } },
        { entries: [{ url: "http://example.org#3" }], extData: { "uniq": r() } },
        { entries: [{ url: "http://example.org#4" }], extData: { "uniq": r() } }
      ],
      selected: 1
    },
    {
      tabs: [
        { entries: [{ url: "http://example.com#1" }], extData: { "uniq": r() } },
        { entries: [{ url: "http://example.com#2" }], extData: { "uniq": r() } },
        { entries: [{ url: "http://example.com#3" }], extData: { "uniq": r() } },
        { entries: [{ url: "http://example.com#4" }], extData: { "uniq": r() } },
      ],
      selected: 3
    }
  ] };
  let state2 = { windows: [
    {
      tabs: [
        { entries: [{ url: "http://example.org#5" }], extData: { "uniq": r() } },
        { entries: [{ url: "http://example.org#6" }], extData: { "uniq": r() } },
        { entries: [{ url: "http://example.org#7" }], extData: { "uniq": r() } },
        { entries: [{ url: "http://example.org#8" }], extData: { "uniq": r() } }
      ],
      selected: 3
    },
    {
      tabs: [
        { entries: [{ url: "http://example.com#5" }], extData: { "uniq": r() } },
        { entries: [{ url: "http://example.com#6" }], extData: { "uniq": r() } },
        { entries: [{ url: "http://example.com#7" }], extData: { "uniq": r() } },
        { entries: [{ url: "http://example.com#8" }], extData: { "uniq": r() } },
      ],
      selected: 1
    }
  ] };

  // interruptedAfter will be set after the selected tab from each window have loaded.
  let interruptedAfter = 0;
  let loadedWindow1 = false;
  let loadedWindow2 = false;
  let numTabs = state2.windows[0].tabs.length + state2.windows[1].tabs.length;

  let loadCount = 0;
  let promiseRestoringTabs = new Promise(resolve => {
    gProgressListener.setCallback(function (aBrowser, aNeedRestore, aRestoring, aRestored) {
      loadCount++;

      if (aBrowser.currentURI.spec == state1.windows[0].tabs[2].entries[0].url)
        loadedWindow1 = true;
      if (aBrowser.currentURI.spec == state1.windows[1].tabs[0].entries[0].url)
        loadedWindow2 = true;

      if (!interruptedAfter && loadedWindow1 && loadedWindow2) {
        interruptedAfter = loadCount;
        ss.setBrowserState(JSON.stringify(state2));
        return;
      }

      if (loadCount < numTabs + interruptedAfter)
        return;

      // We don't actually care about load order in this test, just that they all
      // do load.
      is(loadCount, numTabs + interruptedAfter, "all tabs were restored");
      is(aNeedRestore, 0, "there are no tabs left needing restore");

      // Remove the progress listener.
      gProgressListener.unsetCallback();
      resolve();
    });
  });

  // We also want to catch the extra windows (there should be 2), so we need to observe domwindowopened
  Services.ww.registerNotification(function observer(aSubject, aTopic, aData) {
    if (aTopic == "domwindowopened") {
      let win = aSubject.QueryInterface(Ci.nsIDOMWindow);
      win.addEventListener("load", function onLoad() {
        win.removeEventListener("load", onLoad);
        Services.ww.unregisterNotification(observer);
        win.gBrowser.addTabsProgressListener(gProgressListener);
      });
    }
  });

  let backupState = ss.getBrowserState();
  ss.setBrowserState(JSON.stringify(state1));
  yield promiseRestoringTabs;

  // Cleanup.
  yield promiseAllButPrimaryWindowClosed();
  yield promiseBrowserState(backupState);
});
