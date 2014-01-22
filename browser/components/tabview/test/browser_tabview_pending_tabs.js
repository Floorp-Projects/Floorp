/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const STATE = {
  windows: [{
    tabs: [{
      entries: [{ url: "about:mozilla" }],
      hidden: true,
      extData: {"tabview-tab": '{"url":"about:mozilla","groupID":1}'}
    },{
      entries: [{ url: "about:robots" }],
      hidden: false,
      extData: {"tabview-tab": '{"url":"about:robots","groupID":1}'},
    }],
    selected: 1,
    extData: {
      "tabview-groups": '{"nextID":2,"activeGroupId":1, "totalNumber":1}',
      "tabview-group":
        '{"1":{"bounds":{"left":15,"top":5,"width":280,"height":232},"id":1}}'
    }
  }]
};

/**
 * Make sure that tabs are restored on demand as otherwise the tab will start
 * loading immediately and we can't check whether it shows cached data.
 */
add_task(function setup() {
  Services.prefs.setBoolPref("browser.sessionstore.restore_on_demand", true);

  registerCleanupFunction(() => {
    Services.prefs.clearUserPref("browser.sessionstore.restore_on_demand");
  });
});

/**
 * Ensure that a pending tab shows cached data.
 */
add_task(function () {
  // Open a new window.
  let win = OpenBrowserWindow();
  yield promiseDelayedStartupFinished(win);

  // Set the window to a specific state.
  let ss = Cc["@mozilla.org/browser/sessionstore;1"]
             .getService(Ci.nsISessionStore)
             .setWindowState(win, JSON.stringify(STATE), true);

  // Open Panorama.
  yield promiseTabViewShown(win);

  let [tab1, tab2] = win.gBrowser.tabs;
  let cw = win.TabView.getContentWindow();

  // Update the two tabs in reverse order. Panorama will first try to update
  // the second tab but will put it back onto the queue once it detects that
  // it hasn't loaded yet. It will then try to update the first tab.
  cw.TabItems.update(tab2);
  cw.TabItems.update(tab1);

  let tabItem1 = tab1._tabViewTabItem;
  let tabItem2 = tab2._tabViewTabItem;

  // Wait for the first tabItem to be updated. Calling update() on the second
  // tabItem won't send a notification as that is pushed back onto the queue.
  yield promiseTabItemUpdated(tabItem1);

  // Check that the first tab doesn't show cached data, the second one does.
  ok(!tabItem1.isShowingCachedData(), "doesn't show cached data");
  ok(tabItem2.isShowingCachedData(), "shows cached data");

  // Cleanup.
  yield promiseWindowClosed(win);
});

function promiseTabItemUpdated(tabItem) {
  let deferred = Promise.defer();

  tabItem.addSubscriber("updated", function onUpdated() {
    tabItem.removeSubscriber("updated", onUpdated);
    deferred.resolve();
  });

  return deferred.promise;
}

function promiseAllTabItemsUpdated(win) {
  let deferred = Promise.defer();
  afterAllTabItemsUpdated(deferred.resolve, win);
  return deferred.promise;
}

function promiseDelayedStartupFinished(win) {
  let deferred = Promise.defer();
  whenDelayedStartupFinished(win, deferred.resolve);
  return deferred.promise;
}

function promiseTabViewShown(win) {
  let deferred = Promise.defer();
  showTabView(deferred.resolve, win);
  return deferred.promise;
}

function promiseWindowClosed(win) {
  let deferred = Promise.defer();

  Services.obs.addObserver(function obs(subject, topic) {
    if (subject == win) {
      Services.obs.removeObserver(obs, topic);
      deferred.resolve();
    }
  }, "domwindowclosed", false);

  win.close();
  return deferred.promise;
}
