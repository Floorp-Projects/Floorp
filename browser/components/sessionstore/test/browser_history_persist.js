/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * Ensure that history entries that should not be persisted are restored in the
 * same state.
 */
add_task(async function check_history_not_persisted() {
  // Create an about:blank tab
  let tab = BrowserTestUtils.addTab(gBrowser, "about:blank");
  let browser = tab.linkedBrowser;
  await promiseBrowserLoaded(browser);

  // Retrieve the tab state.
  await TabStateFlusher.flush(browser);
  let state = JSON.parse(ss.getTabState(tab));
  ok(!state.entries[0].persist, "Should have collected the persistence state");
  BrowserTestUtils.removeTab(tab);
  browser = null;

  // Open a new tab to restore into.
  tab = BrowserTestUtils.addTab(gBrowser, "about:blank");
  browser = tab.linkedBrowser;
  await promiseTabState(tab, state);

  if (!SpecialPowers.Services.appinfo.sessionHistoryInParent) {
    await SpecialPowers.spawn(browser, [], function() {
      let sessionHistory =
        docShell.browsingContext.childSessionHistory.legacySHistory;

      is(sessionHistory.count, 1, "Should be a single history entry");
      is(
        sessionHistory.getEntryAtIndex(0).URI.spec,
        "about:blank",
        "Should be the right URL"
      );
    });
  } else {
    let sessionHistory = browser.browsingContext.sessionHistory;

    is(sessionHistory.count, 1, "Should be a single history entry");
    is(
      sessionHistory.getEntryAtIndex(0).URI.spec,
      "about:blank",
      "Should be the right URL"
    );
  }

  // Load a new URL into the tab, it should replace the about:blank history entry
  BrowserTestUtils.loadURI(browser, "about:robots");
  await promiseBrowserLoaded(browser, false, "about:robots");
  if (!SpecialPowers.Services.appinfo.sessionHistoryInParent) {
    await SpecialPowers.spawn(browser, [], function() {
      let sessionHistory =
        docShell.browsingContext.childSessionHistory.legacySHistory;

      is(sessionHistory.count, 1, "Should be a single history entry");
      is(
        sessionHistory.getEntryAtIndex(0).URI.spec,
        "about:robots",
        "Should be the right URL"
      );
    });
  } else {
    let sessionHistory = browser.browsingContext.sessionHistory;

    is(sessionHistory.count, 1, "Should be a single history entry");
    is(
      sessionHistory.getEntryAtIndex(0).URI.spec,
      "about:robots",
      "Should be the right URL"
    );
  }

  // Cleanup.
  BrowserTestUtils.removeTab(tab);
});

/**
 * Check that entries default to being persisted when the attribute doesn't
 * exist
 */
add_task(async function check_history_default_persisted() {
  // Create an about:blank tab
  let tab = BrowserTestUtils.addTab(gBrowser, "about:blank");
  let browser = tab.linkedBrowser;
  await promiseBrowserLoaded(browser);

  // Retrieve the tab state.
  await TabStateFlusher.flush(browser);
  let state = JSON.parse(ss.getTabState(tab));
  delete state.entries[0].persist;
  BrowserTestUtils.removeTab(tab);
  browser = null;

  // Open a new tab to restore into.
  tab = BrowserTestUtils.addTab(gBrowser, "about:blank");
  browser = tab.linkedBrowser;
  await promiseTabState(tab, state);
  if (!SpecialPowers.Services.appinfo.sessionHistoryInParent) {
    await SpecialPowers.spawn(browser, [], function() {
      let sessionHistory =
        docShell.browsingContext.childSessionHistory.legacySHistory;

      is(sessionHistory.count, 1, "Should be a single history entry");
      is(
        sessionHistory.getEntryAtIndex(0).URI.spec,
        "about:blank",
        "Should be the right URL"
      );
    });
  } else {
    let sessionHistory = browser.browsingContext.sessionHistory;

    is(sessionHistory.count, 1, "Should be a single history entry");
    is(
      sessionHistory.getEntryAtIndex(0).URI.spec,
      "about:blank",
      "Should be the right URL"
    );
  }

  // Load a new URL into the tab, it should replace the about:blank history entry
  BrowserTestUtils.loadURI(browser, "about:robots");
  await promiseBrowserLoaded(browser, false, "about:robots");
  if (!SpecialPowers.Services.appinfo.sessionHistoryInParent) {
    await SpecialPowers.spawn(browser, [], function() {
      let sessionHistory =
        docShell.browsingContext.childSessionHistory.legacySHistory;

      is(sessionHistory.count, 2, "Should be two history entries");
      is(
        sessionHistory.getEntryAtIndex(0).URI.spec,
        "about:blank",
        "Should be the right URL"
      );
      is(
        sessionHistory.getEntryAtIndex(1).URI.spec,
        "about:robots",
        "Should be the right URL"
      );
    });
  } else {
    let sessionHistory = browser.browsingContext.sessionHistory;

    is(sessionHistory.count, 2, "Should be two history entries");
    is(
      sessionHistory.getEntryAtIndex(0).URI.spec,
      "about:blank",
      "Should be the right URL"
    );
    is(
      sessionHistory.getEntryAtIndex(1).URI.spec,
      "about:robots",
      "Should be the right URL"
    );
  }

  // Cleanup.
  BrowserTestUtils.removeTab(tab);
});
