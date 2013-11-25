/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

let tmp = {};
Cu.import("resource://gre/modules/Promise.jsm", tmp);
let {Promise} = tmp;

const INITIAL_VALUE = "initial-value-" + Date.now();

/**
 * This test ensures that setting, modifying and restoring sessionStorage data
 * works as expected.
 */
add_task(function session_storage() {
  let tab = yield createTabWithStorageData(["http://example.com", "http://mochi.test:8888"]);
  let browser = tab.linkedBrowser;

  // Flush to make sure chrome received all data.
  SyncHandlers.get(browser).flush();

  let {storage} = JSON.parse(ss.getTabState(tab));
  is(storage["http://example.com"].test, INITIAL_VALUE,
    "sessionStorage data for example.com has been serialized correctly");
  is(storage["http://mochi.test:8888"].test, INITIAL_VALUE,
    "sessionStorage data for mochi.test has been serialized correctly");

  // Ensure that modifying sessionStore values works.
  yield modifySessionStorage(browser, {test: "modified"});
  SyncHandlers.get(browser).flush();

  let {storage} = JSON.parse(ss.getTabState(tab));
  is(storage["http://example.com"].test, INITIAL_VALUE,
    "sessionStorage data for example.com has been serialized correctly");
  is(storage["http://mochi.test:8888"].test, "modified",
    "sessionStorage data for mochi.test has been serialized correctly");

  // Test that duplicating a tab works.
  let tab2 = gBrowser.duplicateTab(tab);
  let browser2 = tab2.linkedBrowser;
  yield promiseTabRestored(tab2);

  // Flush to make sure chrome received all data.
  SyncHandlers.get(browser2).flush();

  let {storage} = JSON.parse(ss.getTabState(tab2));
  is(storage["http://example.com"].test, INITIAL_VALUE,
    "sessionStorage data for example.com has been duplicated correctly");
  is(storage["http://mochi.test:8888"].test, "modified",
    "sessionStorage data for mochi.test has been duplicated correctly");

  // Ensure that the content script retains restored data
  // (by e.g. duplicateTab) and send it along with new data.
  yield modifySessionStorage(browser2, {test: "modified2"});
  SyncHandlers.get(browser2).flush();

  let {storage} = JSON.parse(ss.getTabState(tab2));
  is(storage["http://example.com"].test, INITIAL_VALUE,
    "sessionStorage data for example.com has been duplicated correctly");
  is(storage["http://mochi.test:8888"].test, "modified2",
    "sessionStorage data for mochi.test has been duplicated correctly");

  // Clean up.
  gBrowser.removeTab(tab);
  gBrowser.removeTab(tab2);
});

/**
 * This test ensures that purging domain data also purges data from the
 * sessionStorage data collected for tabs.
 */
add_task(function purge_domain() {
  let tab = yield createTabWithStorageData(["http://example.com", "http://mochi.test:8888"]);
  let browser = tab.linkedBrowser;

  yield notifyObservers(browser, "browser:purge-domain-data", "mochi.test");

  // Flush to make sure chrome received all data.
  SyncHandlers.get(browser).flush();

  let {storage} = JSON.parse(ss.getTabState(tab));
  ok(!storage["http://mochi.test:8888"],
    "sessionStorage data for mochi.test has been purged");
  is(storage["http://example.com"].test, INITIAL_VALUE,
    "sessionStorage data for example.com has been preserved");

  gBrowser.removeTab(tab);
});

/**
 * This test ensures that purging session history data also purges data from
 * sessionStorage data collected for tabs
 */
add_task(function purge_shistory() {
  let tab = yield createTabWithStorageData(["http://example.com", "http://mochi.test:8888"]);
  let browser = tab.linkedBrowser;

  yield notifyObservers(browser, "browser:purge-session-history");

  // Flush to make sure chrome received all data.
  SyncHandlers.get(browser).flush();

  let {storage} = JSON.parse(ss.getTabState(tab));
  ok(!storage["http://example.com"],
    "sessionStorage data for example.com has been purged");
  is(storage["http://mochi.test:8888"].test, INITIAL_VALUE,
    "sessionStorage data for mochi.test has been preserved");

  gBrowser.removeTab(tab);
});

/**
 * This test ensures that collecting sessionStorage data respects the privacy
 * levels as set by the user.
 */
add_task(function respect_privacy_level() {
  let tab = yield createTabWithStorageData(["http://example.com", "https://example.com"]);
  gBrowser.removeTab(tab);

  let [{state: {storage}}] = JSON.parse(ss.getClosedTabData(window));
  is(storage["http://example.com"].test, INITIAL_VALUE,
    "http sessionStorage data has been saved");
  is(storage["https://example.com"].test, INITIAL_VALUE,
    "https sessionStorage data has been saved");

  // Disable saving data for encrypted sites.
  Services.prefs.setIntPref("browser.sessionstore.privacy_level", 1);

  let tab = yield createTabWithStorageData(["http://example.com", "https://example.com"]);
  gBrowser.removeTab(tab);

  let [{state: {storage}}] = JSON.parse(ss.getClosedTabData(window));
  is(storage["http://example.com"].test, INITIAL_VALUE,
    "http sessionStorage data has been saved");
  ok(!storage["https://example.com"],
    "https sessionStorage data has *not* been saved");

  // Disable saving data for any site.
  Services.prefs.setIntPref("browser.sessionstore.privacy_level", 2);

  // Check that duplicating a tab copies all private data.
  let tab = yield createTabWithStorageData(["http://example.com", "https://example.com"]);
  let tab2 = gBrowser.duplicateTab(tab);
  yield promiseBrowserLoaded(tab2.linkedBrowser);
  gBrowser.removeTab(tab);

  // With privacy_level=2 the |tab| shouldn't have any sessionStorage data.
  let [{state: {storage}}] = JSON.parse(ss.getClosedTabData(window));
  ok(!storage["http://example.com"],
    "http sessionStorage data has *not* been saved");
  ok(!storage["https://example.com"],
    "https sessionStorage data has *not* been saved");

  // Restore the default privacy level and close the duplicated tab.
  Services.prefs.clearUserPref("browser.sessionstore.privacy_level");
  gBrowser.removeTab(tab2);

  // With privacy_level=0 the duplicated |tab2| should persist all data.
  let [{state: {storage}}] = JSON.parse(ss.getClosedTabData(window));
  is(storage["http://example.com"].test, INITIAL_VALUE,
    "http sessionStorage data has been saved");
  is(storage["https://example.com"].test, INITIAL_VALUE,
    "https sessionStorage data has been saved");
});

function createTabWithStorageData(urls) {
  return Task.spawn(function task() {
    let tab = gBrowser.addTab();
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

function waitForUpdateMessage(browser) {
  return promiseContentMessage(browser, "SessionStore:update");
}

function modifySessionStorage(browser, data) {
  browser.messageManager.sendAsyncMessage("ss-test:modifySessionStorage", data);
  return waitForStorageEvent(browser);
}

function notifyObservers(browser, topic, data) {
  let msg = {topic: topic, data: data};
  browser.messageManager.sendAsyncMessage("ss-test:notifyObservers", msg);
  return waitForUpdateMessage(browser);
}
