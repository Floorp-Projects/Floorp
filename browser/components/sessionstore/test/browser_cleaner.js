/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */


/*
 * This test ensures that Session Restore eventually forgets about
 * tabs and windows that have been closed a long time ago.
 */

"use strict";

Cu.import("resource://gre/modules/Services.jsm", this);
Cu.import("resource://gre/modules/osfile.jsm", this);
Cu.import("resource://gre/modules/Task.jsm", this);

const LONG_TIME_AGO = 1;

const URL_TAB1 = "http://example.com/browser_cleaner.js?newtab1=" + Math.random();
const URL_TAB2 = "http://example.com/browser_cleaner.js?newtab2=" + Math.random();
const URL_NEWWIN = "http://example.com/browser_cleaner.js?newwin=" + Math.random();

function isRecent(stamp) {
  is(typeof stamp, "number", "This is a timestamp");
  return Date.now() - stamp <= 60000;
}

function promiseCleanup() {
  info("Cleaning up browser");

  return promiseBrowserState(getClosedState());
}

function getClosedState() {
  return Cu.cloneInto(CLOSED_STATE, {});
}

var CLOSED_STATE;

add_task(function* init() {
  forgetClosedWindows();
  while (ss.getClosedTabCount(window) > 0) {
    ss.forgetClosedTab(window, 0);
  }
});

add_task(function* test_open_and_close() {
  let newTab1 = gBrowser.addTab(URL_TAB1);
  yield promiseBrowserLoaded(newTab1.linkedBrowser);

  let newTab2 = gBrowser.addTab(URL_TAB2);
  yield promiseBrowserLoaded(newTab2.linkedBrowser);

  let newWin = yield promiseNewWindowLoaded();
  let tab = newWin.gBrowser.addTab(URL_NEWWIN);

  yield promiseBrowserLoaded(tab.linkedBrowser);

  yield TabStateFlusher.flushWindow(window);
  yield TabStateFlusher.flushWindow(newWin);

  info("1. Making sure that before closing, we don't have closedAt");
  // For the moment, no "closedAt"
  let state = JSON.parse(ss.getBrowserState());
  is(state.windows[0].closedAt || false, false, "1. Main window doesn't have closedAt");
  is(state.windows[1].closedAt || false, false, "1. Second window doesn't have closedAt");
  is(state.windows[0].tabs[0].closedAt || false, false, "1. First tab doesn't have closedAt");
  is(state.windows[0].tabs[1].closedAt || false, false, "1. Second tab doesn't have closedAt");

  info("2. Making sure that after closing, we have closedAt");

  // Now close stuff, this should add closeAt
  yield BrowserTestUtils.closeWindow(newWin);
  yield promiseRemoveTab(newTab1);
  yield promiseRemoveTab(newTab2);

  state = CLOSED_STATE = JSON.parse(ss.getBrowserState());

  is(state.windows[0].closedAt || false, false, "2. Main window doesn't have closedAt");
  ok(isRecent(state._closedWindows[0].closedAt), "2. Second window was closed recently");
  ok(isRecent(state.windows[0]._closedTabs[0].closedAt), "2. First tab was closed recently");
  ok(isRecent(state.windows[0]._closedTabs[1].closedAt), "2. Second tab was closed recently");
});


add_task(function* test_restore() {
  info("3. Making sure that after restoring, we don't have closedAt");
  yield promiseBrowserState(CLOSED_STATE);

  let newWin = ss.undoCloseWindow(0);
  yield promiseDelayedStartupFinished(newWin);

  let newTab2 = ss.undoCloseTab(window, 0);
  yield promiseTabRestored(newTab2);

  let newTab1 = ss.undoCloseTab(window, 0);
  yield promiseTabRestored(newTab1);

  let state = JSON.parse(ss.getBrowserState());

  is(state.windows[0].closedAt || false, false, "3. Main window doesn't have closedAt");
  is(state.windows[1].closedAt || false, false, "3. Second window doesn't have closedAt");
  is(state.windows[0].tabs[0].closedAt || false, false, "3. First tab doesn't have closedAt");
  is(state.windows[0].tabs[1].closedAt || false, false, "3. Second tab doesn't have closedAt");

  yield BrowserTestUtils.closeWindow(newWin);
  gBrowser.removeTab(newTab1);
  gBrowser.removeTab(newTab2);
});


add_task(function* test_old_data() {
  info("4. Removing closedAt from the sessionstore, making sure that it is added upon idle-daily");

  let state = getClosedState();
  delete state._closedWindows[0].closedAt;
  delete state.windows[0]._closedTabs[0].closedAt;
  delete state.windows[0]._closedTabs[1].closedAt;
  yield promiseBrowserState(state);

  info("Sending idle-daily");
  Services.obs.notifyObservers(null, "idle-daily");
  info("Sent idle-daily");

  state = JSON.parse(ss.getBrowserState());
  is(state.windows[0].closedAt || false, false, "4. Main window doesn't have closedAt");
  ok(isRecent(state._closedWindows[0].closedAt), "4. Second window was closed recently");
  ok(isRecent(state.windows[0]._closedTabs[0].closedAt), "4. First tab was closed recently");
  ok(isRecent(state.windows[0]._closedTabs[1].closedAt), "4. Second tab was closed recently");
  yield promiseCleanup();
});


add_task(function* test_cleanup() {

  info("5. Altering closedAt to an old date, making sure that stuff gets collected, eventually");
  yield promiseCleanup();

  let state = getClosedState();
  state._closedWindows[0].closedAt = LONG_TIME_AGO;
  state.windows[0]._closedTabs[0].closedAt = LONG_TIME_AGO;
  state.windows[0]._closedTabs[1].closedAt = Date.now();
  let url = state.windows[0]._closedTabs[1].state.entries[0].url;

  yield promiseBrowserState(state);

  info("Sending idle-daily");
  Services.obs.notifyObservers(null, "idle-daily");
  info("Sent idle-daily");

  state = JSON.parse(ss.getBrowserState());
  is(state._closedWindows[0], undefined, "5. Second window was forgotten");

  is(state.windows[0]._closedTabs.length, 1, "5. Only one closed tab left");
  is(state.windows[0]._closedTabs[0].state.entries[0].url, url, "5. The second tab is still here");
  yield promiseCleanup();
});

