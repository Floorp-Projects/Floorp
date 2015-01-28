"use strict";

const BASE = "http://example.com/browser/browser/components/sessionstore/test/";
const TARGET = BASE + "restore_redirect_target.html";

/**
 * Ensure that a http redirect leaves a working tab.
 */
add_task(function check_http_redirect() {
  let state = {
    entries: [{ url: BASE + "restore_redirect_http.html" }]
  };

  // Open a new tab to restore into.
  let tab = gBrowser.addTab("about:blank");
  let browser = tab.linkedBrowser;
  yield promiseTabState(tab, state);

  info("Restored tab");

  TabState.flush(browser);
  let data = TabState.collect(tab);
  is(data.entries.length, 1, "Should be one entry in session history");
  is(data.entries[0].url, TARGET, "Should be the right session history entry");

  ok(!("__SS_data" in browser), "Temporary restore data should have been cleared");

  // Cleanup.
  gBrowser.removeTab(tab);
});

/**
 * Ensure that a js redirect leaves a working tab.
 */
add_task(function check_js_redirect() {
  let state = {
    entries: [{ url: BASE + "restore_redirect_js.html" }]
  };

  let loadPromise = new Promise(resolve => {
    function listener(msg) {
      if (msg.data.url.endsWith("restore_redirect_target.html")) {
        window.messageManager.removeMessageListener("ss-test:loadEvent", listener);
        resolve();
      }
    }

    window.messageManager.addMessageListener("ss-test:loadEvent", listener);
  });

  // Open a new tab to restore into.
  let tab = gBrowser.addTab("about:blank");
  let browser = tab.linkedBrowser;
  yield promiseTabState(tab, state);

  info("Restored tab");

  yield loadPromise;

  TabState.flush(browser);
  let data = TabState.collect(tab);
  is(data.entries.length, 1, "Should be one entry in session history");
  is(data.entries[0].url, TARGET, "Should be the right session history entry");

  ok(!("__SS_data" in browser), "Temporary restore data should have been cleared");

  // Cleanup.
  gBrowser.removeTab(tab);
});
