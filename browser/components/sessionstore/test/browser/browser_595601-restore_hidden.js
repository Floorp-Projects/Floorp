/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

const TAB_STATE_NEEDS_RESTORE = 1;
const TAB_STATE_RESTORING = 2;

let stateBackup = ss.getBrowserState();

let state = {windows:[{tabs:[
  {entries:[{url:"http://example.com#1"}]},
  {entries:[{url:"http://example.com#2"}]},
  {entries:[{url:"http://example.com#3"}]},
  {entries:[{url:"http://example.com#4"}]},
  {entries:[{url:"http://example.com#5"}], hidden: true},
  {entries:[{url:"http://example.com#6"}], hidden: true},
  {entries:[{url:"http://example.com#7"}], hidden: true},
  {entries:[{url:"http://example.com#8"}], hidden: true}
]}]};

function test() {
  waitForExplicitFinish();

  registerCleanupFunction(function () {
    Services.prefs.clearUserPref("browser.sessionstore.restore_hidden_tabs");

    TabsProgressListener.uninit();

    ss.setBrowserState(stateBackup);
  });

  TabsProgressListener.init();

  // First stage: restoreHiddenTabs = true
  // Second stage: restoreHiddenTabs = false
  test_loadTabs(true, function () {
    test_loadTabs(false, function () {
      waitForFocus(finish);
    });
  });
}

function test_loadTabs(restoreHiddenTabs, callback) {
  Services.prefs.setBoolPref("browser.sessionstore.restore_hidden_tabs", restoreHiddenTabs);

  let expectedTabs = restoreHiddenTabs ? 8 : 4;

  let firstProgress = true;

  TabsProgressListener.setCallback(function (needsRestore, isRestoring) {
    if (firstProgress) {
      firstProgress = false;
      is(isRestoring, 3, "restoring 3 tabs concurrently");
    } else {
      ok(isRestoring < 4, "restoring max. 3 tabs concurrently");
    }

    if (gBrowser.tabs.length - needsRestore == expectedTabs) {
      TabsProgressListener.unsetCallback();
      is(gBrowser.visibleTabs.length, 4, "only 4 visible tabs");
      callback();
    }
  });

  ss.setBrowserState(JSON.stringify(state));
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

let TabsProgressListener = {
  init: function () {
    gBrowser.addTabsProgressListener(this);
  },

  uninit: function () {
    this.unsetCallback();
    gBrowser.removeTabsProgressListener(this);
  },

  setCallback: function (callback) {
    this.callback = callback;
  },

  unsetCallback: function () {
    delete this.callback;
  },

  onStateChange: function (aBrowser, aWebProgress, aRequest, aStateFlags, aStatus) {
    if (this.callback && aBrowser.__SS_restoreState == TAB_STATE_RESTORING &&
        aStateFlags & Ci.nsIWebProgressListener.STATE_STOP &&
        aStateFlags & Ci.nsIWebProgressListener.STATE_IS_NETWORK &&
        aStateFlags & Ci.nsIWebProgressListener.STATE_IS_WINDOW)
      this.callback.apply(null, countTabs());
  }
}
