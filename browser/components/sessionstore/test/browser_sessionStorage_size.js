/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const RAND = Math.random();
const URL = "http://mochi.test:8888/browser/" +
            "browser/components/sessionstore/test/browser_sessionStorage.html" +
            "?" + RAND;

const OUTER_VALUE = "outer-value-" + RAND;

// Test that we record the size of messages.
add_task(function* test_telemetry() {
  Services.telemetry.canRecordExtended = true;
  let histogram = Services.telemetry.getHistogramById("FX_SESSION_RESTORE_DOM_STORAGE_SIZE_ESTIMATE_CHARS");
  let snap1 = histogram.snapshot();

  let tab = gBrowser.addTab(URL);
  let browser = tab.linkedBrowser;
  yield promiseBrowserLoaded(browser);

  // Flush to make sure chrome received all data.
  yield TabStateFlusher.flush(browser);
  let snap2 = histogram.snapshot();

  Assert.ok(snap2.counts[5] > snap1.counts[5]);
  yield promiseRemoveTab(tab);
  Services.telemetry.canRecordExtended = false;
});

// Lower the size limit for DOM Storage content. Check that DOM Storage
// is not updated, but that other things remain updated.
add_task(function* test_large_content() {
  Services.prefs.setIntPref("browser.sessionstore.dom_storage_limit", 5);

  let tab = gBrowser.addTab(URL);
  let browser = tab.linkedBrowser;
  yield promiseBrowserLoaded(browser);

  // Flush to make sure chrome received all data.
  yield TabStateFlusher.flush(browser);

  let state = JSON.parse(ss.getTabState(tab));
  info(JSON.stringify(state, null, "\t"));
  Assert.equal(state.storage, null, "We have no storage for the tab");
  Assert.equal(state.entries[0].title, OUTER_VALUE);
  yield promiseRemoveTab(tab);

  Services.prefs.clearUserPref("browser.sessionstore.dom_storage_limit");
});
