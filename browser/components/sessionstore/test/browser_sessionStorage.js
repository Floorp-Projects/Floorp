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
add_task(function session_storage() {
  let tab = gBrowser.addTab(URL);
  let browser = tab.linkedBrowser;
  yield promiseBrowserLoaded(browser);

  // Flush to make sure chrome received all data.
  TabState.flush(browser);

  let {storage} = JSON.parse(ss.getTabState(tab));
  is(storage["http://example.com"].test, INNER_VALUE,
    "sessionStorage data for example.com has been serialized correctly");
  is(storage["http://mochi.test:8888"].test, OUTER_VALUE,
    "sessionStorage data for mochi.test has been serialized correctly");

  // Ensure that modifying sessionStore values works for the inner frame only.
  yield modifySessionStorage2(browser, {test: "modified1"});
  TabState.flush(browser);

  ({storage} = JSON.parse(ss.getTabState(tab)));
  is(storage["http://example.com"].test, "modified1",
    "sessionStorage data for example.com has been serialized correctly");
  is(storage["http://mochi.test:8888"].test, OUTER_VALUE,
    "sessionStorage data for mochi.test has been serialized correctly");

  // Ensure that modifying sessionStore values works for both frames.
  yield modifySessionStorage(browser, {test: "modified"});
  yield modifySessionStorage2(browser, {test: "modified2"});
  TabState.flush(browser);

  ({storage} = JSON.parse(ss.getTabState(tab)));
  is(storage["http://example.com"].test, "modified2",
    "sessionStorage data for example.com has been serialized correctly");
  is(storage["http://mochi.test:8888"].test, "modified",
    "sessionStorage data for mochi.test has been serialized correctly");

  // Test that duplicating a tab works.
  let tab2 = gBrowser.duplicateTab(tab);
  let browser2 = tab2.linkedBrowser;
  yield promiseTabRestored(tab2);

  // Flush to make sure chrome received all data.
  TabState.flush(browser2);

  ({storage} = JSON.parse(ss.getTabState(tab2)));
  is(storage["http://example.com"].test, "modified2",
    "sessionStorage data for example.com has been duplicated correctly");
  is(storage["http://mochi.test:8888"].test, "modified",
    "sessionStorage data for mochi.test has been duplicated correctly");

  // Ensure that the content script retains restored data
  // (by e.g. duplicateTab) and sends it along with new data.
  yield modifySessionStorage(browser2, {test: "modified3"});
  TabState.flush(browser2);

  ({storage} = JSON.parse(ss.getTabState(tab2)));
  is(storage["http://example.com"].test, "modified2",
    "sessionStorage data for example.com has been duplicated correctly");
  is(storage["http://mochi.test:8888"].test, "modified3",
    "sessionStorage data for mochi.test has been duplicated correctly");

  // Check that loading a new URL discards data.
  browser2.loadURI("http://mochi.test:8888/");
  yield promiseBrowserLoaded(browser2);
  TabState.flush(browser2);

  ({storage} = JSON.parse(ss.getTabState(tab2)));
  is(storage["http://mochi.test:8888"].test, "modified3",
    "navigating retains correct storage data");
  ok(!storage["http://example.com"], "storage data was discarded");

  // Check that loading a new URL discards data.
  browser2.loadURI("about:mozilla");
  yield promiseBrowserLoaded(browser2);
  TabState.flush(browser2);

  let state = JSON.parse(ss.getTabState(tab2));
  ok(!state.hasOwnProperty("storage"), "storage data was discarded");

  // Clean up.
  gBrowser.removeTab(tab);
  gBrowser.removeTab(tab2);
});

/**
 * This test ensures that purging domain data also purges data from the
 * sessionStorage data collected for tabs.
 */
add_task(function purge_domain() {
  let tab = gBrowser.addTab(URL);
  let browser = tab.linkedBrowser;
  yield promiseBrowserLoaded(browser);

  // Purge data for "mochi.test".
  yield purgeDomainData(browser, "mochi.test");

  // Flush to make sure chrome received all data.
  TabState.flush(browser);

  let {storage} = JSON.parse(ss.getTabState(tab));
  ok(!storage["http://mochi.test:8888"],
    "sessionStorage data for mochi.test has been purged");
  is(storage["http://example.com"].test, INNER_VALUE,
    "sessionStorage data for example.com has been preserved");

  gBrowser.removeTab(tab);
});

/**
 * This test ensures that collecting sessionStorage data respects the privacy
 * levels as set by the user.
 */
add_task(function respect_privacy_level() {
  let tab = gBrowser.addTab(URL + "&secure");
  yield promiseBrowserLoaded(tab.linkedBrowser);
  yield promiseRemoveTab(tab);

  let [{state: {storage}}] = JSON.parse(ss.getClosedTabData(window));
  is(storage["http://mochi.test:8888"].test, OUTER_VALUE,
    "http sessionStorage data has been saved");
  is(storage["https://example.com"].test, INNER_VALUE,
    "https sessionStorage data has been saved");

  // Disable saving data for encrypted sites.
  Services.prefs.setIntPref("browser.sessionstore.privacy_level", 1);

  tab = gBrowser.addTab(URL + "&secure");
  yield promiseBrowserLoaded(tab.linkedBrowser);
  yield promiseRemoveTab(tab);

  [{state: {storage}}] = JSON.parse(ss.getClosedTabData(window));
  is(storage["http://mochi.test:8888"].test, OUTER_VALUE,
    "http sessionStorage data has been saved");
  ok(!storage["https://example.com"],
    "https sessionStorage data has *not* been saved");

  // Disable saving data for any site.
  Services.prefs.setIntPref("browser.sessionstore.privacy_level", 2);

  // Check that duplicating a tab copies all private data.
  tab = gBrowser.addTab(URL + "&secure");
  yield promiseBrowserLoaded(tab.linkedBrowser);
  let tab2 = gBrowser.duplicateTab(tab);
  yield promiseTabRestored(tab2);
  yield promiseRemoveTab(tab);

  // With privacy_level=2 the |tab| shouldn't have any sessionStorage data.
  [{state: {storage}}] = JSON.parse(ss.getClosedTabData(window));
  ok(!storage, "sessionStorage data has *not* been saved");

  // Restore the default privacy level and close the duplicated tab.
  Services.prefs.clearUserPref("browser.sessionstore.privacy_level");
  yield promiseRemoveTab(tab2);

  // With privacy_level=0 the duplicated |tab2| should persist all data.
  [{state: {storage}}] = JSON.parse(ss.getClosedTabData(window));
  is(storage["http://mochi.test:8888"].test, OUTER_VALUE,
    "http sessionStorage data has been saved");
  is(storage["https://example.com"].test, INNER_VALUE,
    "https sessionStorage data has been saved");
});

function waitForStorageEvent(browser) {
  return promiseContentMessage(browser, "ss-test:MozStorageChanged");
}

function modifySessionStorage(browser, data) {
  browser.messageManager.sendAsyncMessage("ss-test:modifySessionStorage", data);
  return waitForStorageEvent(browser);
}

function modifySessionStorage2(browser, data) {
  browser.messageManager.sendAsyncMessage("ss-test:modifySessionStorage2", data);
  return waitForStorageEvent(browser);
}

function purgeDomainData(browser, domain) {
  return sendMessage(browser, "ss-test:purgeDomainData", domain);
}
