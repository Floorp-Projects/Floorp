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
  let suffix = gMultiProcessBrowser ? "#content" : "";
  let histogram = Services.telemetry.getHistogramById("FX_SESSION_RESTORE_DOM_STORAGE_SIZE_ESTIMATE_CHARS" + suffix);
  let snap1 = histogram.snapshot();

  let tab = gBrowser.addTab(URL);
  let browser = tab.linkedBrowser;
  yield promiseBrowserLoaded(browser);

  // Flush to make sure we submitted telemetry data.
  yield TabStateFlusher.flush(browser);

  // There is no good way to make sure that the parent received the histogram entries from the child processes.
  // Let's stick to the ugly, spinning the event loop until we have a good approach (Bug 1357509).
  yield BrowserTestUtils.waitForCondition(() => {
    return histogram.snapshot().counts[5] > snap1.counts[5];
  });

  Assert.ok(true);
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
