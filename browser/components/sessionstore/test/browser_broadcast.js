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
  yield promiseRemoveTab(tab);

  let [{state: {storage}}] = JSON.parse(ss.getClosedTabData(window));
  is(storage["http://example.com"].test, "on-tab-close",
    "sessionStorage data has been flushed on TabClose");
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
  yield promiseTabRestored(tab2);

  yield promiseRemoveTab(tab2);
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
  yield BrowserTestUtils.closeWindow(win);

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
  yield TabStateFlusher.flush(browser);

  let state = ss.getTabState(tab);
  yield modifySessionStorage(browser, {test: "on-set-tab-state"});

  // Flush all data contained in the content script but send it using
  // asynchronous messages.
  TabStateFlusher.flush(browser);

  yield promiseTabState(tab, state);

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
  yield TabStateFlusher.flush(browser);

  yield modifySessionStorage(browser, {test: "on-tab-close-racy"});

  // Flush all data contained in the content script but send it using
  // asynchronous messages.
  TabStateFlusher.flush(browser);
  yield promiseRemoveTab(tab);

  let [{state: {storage}}] = JSON.parse(ss.getClosedTabData(window));
  is(storage["http://example.com"].test, "on-tab-close-racy",
    "sessionStorage data has been merged correctly to prevent data loss");
});

function promiseNewWindow() {
  let deferred = Promise.defer();
  whenNewWindowLoaded({private: false}, deferred.resolve);
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
