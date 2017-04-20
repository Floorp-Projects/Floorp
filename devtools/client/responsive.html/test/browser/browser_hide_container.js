/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Ensure that the RDM container tab URL is not recorded in session history.

const TEST_URL = "http://example.com/";
const CONTAINER_URL = "chrome://devtools/content/responsive.html/index.xhtml";

const { TabStateFlusher } =
  Cu.import("resource:///modules/sessionstore/TabStateFlusher.jsm", {});
const SessionStore =
  Cc["@mozilla.org/browser/sessionstore;1"].getService(Ci.nsISessionStore);
const { OUTER_FRAME_LOADER_SYMBOL } =
  require("devtools/client/responsive.html/browser/tunnel");

function flushContainerTabState(tab) {
  let browser = tab.linkedBrowser;
  let outerBrowser = {
    permanentKey: browser.permanentKey,
    messageManager: browser[OUTER_FRAME_LOADER_SYMBOL].messageManager,
  };
  // Try to flush the tab, but if that hangs for a while, resolve anyway.
  // During this test, we actually expect this to hang because the container tab
  // doesn't have the right epoch value to reply to the flush correctly.
  return new Promise(resolve => {
    TabStateFlusher.flush(outerBrowser).then(resolve);
    waitForTime(10000).then(resolve);
  });
}

add_task(function* () {
  // Load test URL
  let tab = yield addTab(TEST_URL);
  let browser = tab.linkedBrowser;

  // Check session history state
  let history = yield getSessionHistory(browser);
  is(history.index - 1, 0, "At page 0 in history");
  is(history.entries.length, 1, "1 page in history");
  is(history.entries[0].url, TEST_URL, "Page 0 URL matches");

  // Open RDM
  yield openRDM(tab);

  // Checking session history directly in content does show the container URL
  // that we're trying to hide...
  history = yield getSessionHistory(browser);
  is(history.index - 1, 0, "At page 0 in history");
  is(history.entries.length, 1, "1 page in history");
  is(history.entries[0].url, CONTAINER_URL, "Page 0 URL matches");

  // However, checking the recorded tab state for the outer browser shows the
  // container URL has been ignored correctly.
  yield flushContainerTabState(tab);
  let tabState = JSON.parse(SessionStore.getTabState(tab));
  is(tabState.index - 1, 0, "At page 0 in history");
  is(tabState.entries.length, 1, "1 page in history");
  is(tabState.entries[0].url, TEST_URL, "Page 0 URL matches");

  yield closeRDM(tab);
  yield removeTab(tab);
});
