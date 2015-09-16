/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

var stateBackup = ss.getBrowserState();

var statePinned = {windows:[{tabs:[
  {entries:[{url:"http://example.com#1"}], pinned: true}
]}]};

var state = {windows:[{tabs:[
  {entries:[{url:"http://example.com#1"}]},
  {entries:[{url:"http://example.com#2"}]},
  {entries:[{url:"http://example.com#3"}]},
  {entries:[{url:"http://example.com#4"}]},
]}]};

function test() {
  waitForExplicitFinish();

  registerCleanupFunction(function () {
    TabsProgressListener.uninit();
    ss.setBrowserState(stateBackup);
  });


  TabsProgressListener.init();

  window.addEventListener("SSWindowStateReady", function onReady() {
    window.removeEventListener("SSWindowStateReady", onReady, false);

    let firstProgress = true;

    TabsProgressListener.setCallback(function (needsRestore, isRestoring) {
      if (firstProgress) {
        firstProgress = false;
        is(isRestoring, 3, "restoring 3 tabs concurrently");
      } else {
        ok(isRestoring <= 3, "restoring max. 2 tabs concurrently");
      }

      if (0 == needsRestore) {
        TabsProgressListener.unsetCallback();
        waitForFocus(finish);
      }
    });

    ss.setBrowserState(JSON.stringify(state));
  }, false);

  ss.setBrowserState(JSON.stringify(statePinned));
}

function countTabs() {
  let needsRestore = 0, isRestoring = 0;
  let windowsEnum = Services.wm.getEnumerator("navigator:browser");

  while (windowsEnum.hasMoreElements()) {
    let window = windowsEnum.getNext();
    if (window.closed)
      continue;

    for (let i = 0; i < window.gBrowser.tabs.length; i++) {
      let browser = window.gBrowser.tabs[i].linkedBrowser;
      if (browser.__SS_restoreState == TAB_STATE_RESTORING)
        isRestoring++;
      else if (browser.__SS_restoreState == TAB_STATE_NEEDS_RESTORE)
        needsRestore++;
    }
  }

  return [needsRestore, isRestoring];
}

var TabsProgressListener = {
  init: function () {
    Services.obs.addObserver(this, "sessionstore-debug-tab-restored", false);
  },

  uninit: function () {
    Services.obs.removeObserver(this, "sessionstore-debug-tab-restored");
    this.unsetCallback();
 },

  setCallback: function (callback) {
    this.callback = callback;
  },

  unsetCallback: function () {
    delete this.callback;
  },

  observe: function (browser, topic, data) {
    TabsProgressListener.onRestored(browser);
  },

  onRestored: function (browser) {
    if (this.callback && browser.__SS_restoreState == TAB_STATE_RESTORING) {
      this.callback.apply(null, countTabs());
    }
  }
}
