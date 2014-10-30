/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * Ensure that history entries that should not be persisted are restored in the
 * same state.
 */
add_task(function check_history_not_persisted() {
  // Create an about:blank tab
  let tab = gBrowser.addTab("about:blank");
  let browser = tab.linkedBrowser;
  yield promiseBrowserLoaded(browser);

  // Retrieve the tab state.
  TabState.flush(browser);
  let state = JSON.parse(ss.getTabState(tab));
  ok(!state.entries[0].persist, "Should have collected the persistence state");
  gBrowser.removeTab(tab);
  browser = null;

  // Open a new tab to restore into.
  tab = gBrowser.addTab("about:blank");
  browser = tab.linkedBrowser;
  yield promiseTabState(tab, state);
  let sessionHistory = browser.sessionHistory;

  is(sessionHistory.count, 1, "Should be a single history entry");
  is(sessionHistory.getEntryAtIndex(0, false).URI.spec, "about:blank", "Should be the right URL");

  // Load a new URL into the tab, it should replace the about:blank history entry
  browser.loadURI("about:robots");
  yield promiseBrowserLoaded(browser);
  sessionHistory = browser.sessionHistory;
  is(sessionHistory.count, 1, "Should be a single history entry");
  is(sessionHistory.getEntryAtIndex(0, false).URI.spec, "about:robots", "Should be the right URL");

  // Cleanup.
  gBrowser.removeTab(tab);
});

/**
 * Check that entries default to being persisted when the attribute doesn't
 * exist
 */
add_task(function check_history_default_persisted() {
  // Create an about:blank tab
  let tab = gBrowser.addTab("about:blank");
  let browser = tab.linkedBrowser;
  yield promiseBrowserLoaded(browser);

  // Retrieve the tab state.
  TabState.flush(browser);
  let state = JSON.parse(ss.getTabState(tab));
  delete state.entries[0].persist;
  gBrowser.removeTab(tab);
  browser = null;

  // Open a new tab to restore into.
  tab = gBrowser.addTab("about:blank");
  browser = tab.linkedBrowser;
  yield promiseTabState(tab, state);
  let sessionHistory = browser.sessionHistory;

  is(sessionHistory.count, 1, "Should be a single history entry");
  is(sessionHistory.getEntryAtIndex(0, false).URI.spec, "about:blank", "Should be the right URL");

  // Load a new URL into the tab, it should replace the about:blank history entry
  browser.loadURI("about:robots");
  yield promiseBrowserLoaded(browser);
  sessionHistory = browser.sessionHistory;
  is(sessionHistory.count, 2, "Should be two history entries");
  is(sessionHistory.getEntryAtIndex(0, false).URI.spec, "about:blank", "Should be the right URL");
  is(sessionHistory.getEntryAtIndex(1, false).URI.spec, "about:robots", "Should be the right URL");

  // Cleanup.
  gBrowser.removeTab(tab);
});
