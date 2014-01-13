/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const INITIAL_VALUE = "browser_broadcast.js-initial-value-" + Date.now();

/**
 * This test ensures we won't lose tab data queued in the content script when
 * closing a tab.
 */
add_task(function flush_on_tabclose() {
  let tab = yield createTabWithStorageData(["http://example.com"]);
  let browser = tab.linkedBrowser;

  yield modifySessionStorage(browser, {test: "on-tab-close"});
  gBrowser.removeTab(tab);

  let [{state: {storage}}] = JSON.parse(ss.getClosedTabData(window));
  is(storage["http://example.com"].test, "on-tab-close",
    "sessionStorage data has been flushed on TabClose");
});

/**
 * This test ensures we won't lose tab data queued in the content script when
 * the application tries to quit.
 */
add_task(function flush_on_quit_requested() {
  let tab = yield createTabWithStorageData(["http://example.com"]);
  let browser = tab.linkedBrowser;

  yield modifySessionStorage(browser, {test: "on-quit-requested"});

  // Note that sending quit-application-requested should not interfere with
  // other tests and code. We're just notifying about a shutdown request but
  // we will not send quit-application-granted. Observers will thus assume
  // that some other observer has canceled the request.
  sendQuitApplicationRequested();

  let {storage} = JSON.parse(ss.getTabState(tab));
  is(storage["http://example.com"].test, "on-quit-requested",
    "sessionStorage data has been flushed when a quit is requested");

  gBrowser.removeTab(tab);
});

/**
 * This test ensures we won't lose tab data queued in the content script when
 * duplicating a tab.
 */
add_task(function flush_on_duplicate() {
  let tab = yield createTabWithStorageData(["http://example.com"]);
  let browser = tab.linkedBrowser;

  yield modifySessionStorage(browser, {test: "on-duplicate"});
  let tab2 = ss.duplicateTab(window, tab);
  let {storage} = JSON.parse(ss.getTabState(tab2));
  is(storage["http://example.com"].test, "on-duplicate",
    "sessionStorage data has been flushed when duplicating tabs");

  yield promiseTabRestored(tab2);
  gBrowser.removeTab(tab2)
  let [{state: {storage}}] = JSON.parse(ss.getClosedTabData(window));
  is(storage["http://example.com"].test, "on-duplicate",
    "sessionStorage data has been flushed when duplicating tabs");

  gBrowser.removeTab(tab);
});

/**
 * This test ensures we won't lose tab data queued in the content script when
 * a window is closed.
 */
add_task(function flush_on_windowclose() {
  let win = yield promiseNewWindow();
  let tab = yield createTabWithStorageData(["http://example.com"], win);
  let browser = tab.linkedBrowser;

  yield modifySessionStorage(browser, {test: "on-window-close"});
  yield closeWindow(win);

  let [{tabs: [_, {storage}]}] = JSON.parse(ss.getClosedWindowData());
  is(storage["http://example.com"].test, "on-window-close",
    "sessionStorage data has been flushed when closing a window");
});

/**
 * This test ensures that stale tab data is ignored when reusing a tab
 * (via e.g. setTabState) and does not overwrite the new data.
 */
add_task(function flush_on_settabstate() {
  let tab = yield createTabWithStorageData(["http://example.com"]);
  let browser = tab.linkedBrowser;

  // Flush to make sure our tab state is up-to-date.
  SyncHandlers.get(browser).flush();

  let state = ss.getTabState(tab);
  yield modifySessionStorage(browser, {test: "on-set-tab-state"});

  // Flush all data contained in the content script but send it using
  // asynchronous messages.
  SyncHandlers.get(browser).flushAsync();

  ss.setTabState(tab, state);
  yield promiseTabRestored(tab);

  let {storage} = JSON.parse(ss.getTabState(tab));
  is(storage["http://example.com"].test, INITIAL_VALUE,
    "sessionStorage data has not been overwritten");

  gBrowser.removeTab(tab);
});

/**
 * This test ensures that we won't lose tab data that has been sent
 * asynchronously just before closing a tab. Flushing must re-send all data
 * that hasn't been received by chrome, yet.
 */
add_task(function flush_on_tabclose_racy() {
  let tab = yield createTabWithStorageData(["http://example.com"]);
  let browser = tab.linkedBrowser;

  // Flush to make sure we start with an empty queue.
  SyncHandlers.get(browser).flush();

  yield modifySessionStorage(browser, {test: "on-tab-close-racy"});

  // Flush all data contained in the content script but send it using
  // asynchronous messages.
  SyncHandlers.get(browser).flushAsync();
  gBrowser.removeTab(tab);

  let [{state: {storage}}] = JSON.parse(ss.getClosedTabData(window));
  is(storage["http://example.com"].test, "on-tab-close-racy",
    "sessionStorage data has been merged correctly to prevent data loss");
});

function promiseNewWindow() {
  let deferred = Promise.defer();

  whenNewWindowLoaded({private: false}, function (win) {
    win.messageManager.loadFrameScript(FRAME_SCRIPT, true);
    deferred.resolve(win);
  });

  return deferred.promise;
}

function closeWindow(win) {
  let deferred = Promise.defer();
  let outerID = win.QueryInterface(Ci.nsIInterfaceRequestor)
                   .getInterface(Ci.nsIDOMWindowUtils)
                   .outerWindowID;

  Services.obs.addObserver(function obs(subject, topic) {
    let id = subject.QueryInterface(Ci.nsISupportsPRUint64).data;
    if (id == outerID) {
      Services.obs.removeObserver(obs, topic);
      deferred.resolve();
    }
  }, "outer-window-destroyed", false);

  win.close();
  return deferred.promise;
}

function createTabWithStorageData(urls, win = window) {
  return Task.spawn(function task() {
    let tab = win.gBrowser.addTab();
    let browser = tab.linkedBrowser;

    for (let url of urls) {
      browser.loadURI(url);
      yield promiseBrowserLoaded(browser);
      yield modifySessionStorage(browser, {test: INITIAL_VALUE});
    }

    throw new Task.Result(tab);
  });
}

function waitForStorageEvent(browser) {
  return promiseContentMessage(browser, "ss-test:MozStorageChanged");
}

function sendQuitApplicationRequested() {
  let cancelQuit = Cc["@mozilla.org/supports-PRBool;1"]
                     .createInstance(Ci.nsISupportsPRBool);
  Services.obs.notifyObservers(cancelQuit, "quit-application-requested", null);
}

function modifySessionStorage(browser, data) {
  browser.messageManager.sendAsyncMessage("ss-test:modifySessionStorage", data);
  return waitForStorageEvent(browser);
}
