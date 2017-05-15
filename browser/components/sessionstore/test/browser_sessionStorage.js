/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const RAND = Math.random();
const URL = "http://mochi.test:8888/browser/" +
            "browser/components/sessionstore/test/browser_sessionStorage.html" +
            "?" + RAND;

const OUTER_VALUE = "outer-value-" + RAND;
const INNER_VALUE = "inner-value-" + RAND;

/**
 * This test ensures that setting, modifying and restoring sessionStorage data
 * works as expected.
 */
add_task(async function session_storage() {
  let tab = BrowserTestUtils.addTab(gBrowser, URL);
  let browser = tab.linkedBrowser;
  await promiseBrowserLoaded(browser);

  // Flush to make sure chrome received all data.
  await TabStateFlusher.flush(browser);

  let {storage} = JSON.parse(ss.getTabState(tab));
  is(storage["http://example.com"].test, INNER_VALUE,
    "sessionStorage data for example.com has been serialized correctly");
  is(storage["http://mochi.test:8888"].test, OUTER_VALUE,
    "sessionStorage data for mochi.test has been serialized correctly");

  // Ensure that modifying sessionStore values works for the inner frame only.
  await modifySessionStorage(browser, {test: "modified1"}, {frameIndex: 0});
  await TabStateFlusher.flush(browser);

  ({storage} = JSON.parse(ss.getTabState(tab)));
  is(storage["http://example.com"].test, "modified1",
    "sessionStorage data for example.com has been serialized correctly");
  is(storage["http://mochi.test:8888"].test, OUTER_VALUE,
    "sessionStorage data for mochi.test has been serialized correctly");

  // Ensure that modifying sessionStore values works for both frames.
  await modifySessionStorage(browser, {test: "modified"});
  await modifySessionStorage(browser, {test: "modified2"}, {frameIndex: 0});
  await TabStateFlusher.flush(browser);

  ({storage} = JSON.parse(ss.getTabState(tab)));
  is(storage["http://example.com"].test, "modified2",
    "sessionStorage data for example.com has been serialized correctly");
  is(storage["http://mochi.test:8888"].test, "modified",
    "sessionStorage data for mochi.test has been serialized correctly");

  // Test that duplicating a tab works.
  let tab2 = gBrowser.duplicateTab(tab);
  let browser2 = tab2.linkedBrowser;
  await promiseTabRestored(tab2);

  // Flush to make sure chrome received all data.
  await TabStateFlusher.flush(browser2);

  ({storage} = JSON.parse(ss.getTabState(tab2)));
  is(storage["http://example.com"].test, "modified2",
    "sessionStorage data for example.com has been duplicated correctly");
  is(storage["http://mochi.test:8888"].test, "modified",
    "sessionStorage data for mochi.test has been duplicated correctly");

  // Ensure that the content script retains restored data
  // (by e.g. duplicateTab) and sends it along with new data.
  await modifySessionStorage(browser2, {test: "modified3"});
  await TabStateFlusher.flush(browser2);

  ({storage} = JSON.parse(ss.getTabState(tab2)));
  is(storage["http://example.com"].test, "modified2",
    "sessionStorage data for example.com has been duplicated correctly");
  is(storage["http://mochi.test:8888"].test, "modified3",
    "sessionStorage data for mochi.test has been duplicated correctly");

  // Check that loading a new URL discards data.
  browser2.loadURI("http://mochi.test:8888/");
  await promiseBrowserLoaded(browser2);
  await TabStateFlusher.flush(browser2);

  ({storage} = JSON.parse(ss.getTabState(tab2)));
  is(storage["http://mochi.test:8888"].test, "modified3",
    "navigating retains correct storage data");
  ok(!storage["http://example.com"], "storage data was discarded");

  // Check that loading a new URL discards data.
  browser2.loadURI("about:mozilla");
  await promiseBrowserLoaded(browser2);
  await TabStateFlusher.flush(browser2);

  let state = JSON.parse(ss.getTabState(tab2));
  ok(!state.hasOwnProperty("storage"), "storage data was discarded");

  // Clean up.
  await promiseRemoveTab(tab);
  await promiseRemoveTab(tab2);
});

/**
 * This test ensures that purging domain data also purges data from the
 * sessionStorage data collected for tabs.
 */
add_task(async function purge_domain() {
  let tab = BrowserTestUtils.addTab(gBrowser, URL);
  let browser = tab.linkedBrowser;
  await promiseBrowserLoaded(browser);

  // Purge data for "mochi.test".
  await purgeDomainData(browser, "mochi.test");

  // Flush to make sure chrome received all data.
  await TabStateFlusher.flush(browser);

  let {storage} = JSON.parse(ss.getTabState(tab));
  ok(!storage["http://mochi.test:8888"],
    "sessionStorage data for mochi.test has been purged");
  is(storage["http://example.com"].test, INNER_VALUE,
    "sessionStorage data for example.com has been preserved");

  await promiseRemoveTab(tab);
});

/**
 * This test ensures that collecting sessionStorage data respects the privacy
 * levels as set by the user.
 */
add_task(async function respect_privacy_level() {
  let tab = BrowserTestUtils.addTab(gBrowser, URL + "&secure");
  await promiseBrowserLoaded(tab.linkedBrowser);
  await promiseRemoveTab(tab);

  let [{state: {storage}}] = JSON.parse(ss.getClosedTabData(window));
  is(storage["http://mochi.test:8888"].test, OUTER_VALUE,
    "http sessionStorage data has been saved");
  is(storage["https://example.com"].test, INNER_VALUE,
    "https sessionStorage data has been saved");

  // Disable saving data for encrypted sites.
  Services.prefs.setIntPref("browser.sessionstore.privacy_level", 1);

  tab = BrowserTestUtils.addTab(gBrowser, URL + "&secure");
  await promiseBrowserLoaded(tab.linkedBrowser);
  await promiseRemoveTab(tab);

  [{state: {storage}}] = JSON.parse(ss.getClosedTabData(window));
  is(storage["http://mochi.test:8888"].test, OUTER_VALUE,
    "http sessionStorage data has been saved");
  ok(!storage["https://example.com"],
    "https sessionStorage data has *not* been saved");

  // Disable saving data for any site.
  Services.prefs.setIntPref("browser.sessionstore.privacy_level", 2);

  // Check that duplicating a tab copies all private data.
  tab = BrowserTestUtils.addTab(gBrowser, URL + "&secure");
  await promiseBrowserLoaded(tab.linkedBrowser);
  let tab2 = gBrowser.duplicateTab(tab);
  await promiseTabRestored(tab2);
  await promiseRemoveTab(tab);

  // With privacy_level=2 the |tab| shouldn't have any sessionStorage data.
  [{state: {storage}}] = JSON.parse(ss.getClosedTabData(window));
  ok(!storage, "sessionStorage data has *not* been saved");

  // Remove all closed tabs before continuing with the next test.
  // As Date.now() isn't monotonic we might sometimes check
  // the wrong closedTabData entry.
  while (ss.getClosedTabCount(window) > 0) {
    ss.forgetClosedTab(window, 0);
  }

  // Restore the default privacy level and close the duplicated tab.
  Services.prefs.clearUserPref("browser.sessionstore.privacy_level");
  await promiseRemoveTab(tab2);

  // With privacy_level=0 the duplicated |tab2| should persist all data.
  [{state: {storage}}] = JSON.parse(ss.getClosedTabData(window));
  is(storage["http://mochi.test:8888"].test, OUTER_VALUE,
    "http sessionStorage data has been saved");
  is(storage["https://example.com"].test, INNER_VALUE,
    "https sessionStorage data has been saved");
});

function purgeDomainData(browser, domain) {
  return sendMessage(browser, "ss-test:purgeDomainData", domain);
}
