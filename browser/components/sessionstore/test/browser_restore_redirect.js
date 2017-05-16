"use strict";

const BASE = "http://example.com/browser/browser/components/sessionstore/test/";
const TARGET = BASE + "restore_redirect_target.html";

/**
 * Ensure that a http redirect leaves a working tab.
 */
add_task(async function check_http_redirect() {
  let state = {
    entries: [{ url: BASE + "restore_redirect_http.html", triggeringPrincipal_base64}]
  };

  // Open a new tab to restore into.
  let tab = BrowserTestUtils.addTab(gBrowser, "about:blank");
  let browser = tab.linkedBrowser;
  await promiseTabState(tab, state);

  info("Restored tab");

  await TabStateFlusher.flush(browser);
  let data = TabState.collect(tab);
  is(data.entries.length, 1, "Should be one entry in session history");
  is(data.entries[0].url, TARGET, "Should be the right session history entry");

  ok(!("__SS_data" in browser), "Temporary restore data should have been cleared");

  // Cleanup.
  await promiseRemoveTab(tab);
});

/**
 * Ensure that a js redirect leaves a working tab.
 */
add_task(async function check_js_redirect() {
  let state = {
    entries: [{ url: BASE + "restore_redirect_js.html", triggeringPrincipal_base64}]
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
  let tab = BrowserTestUtils.addTab(gBrowser, "about:blank");
  let browser = tab.linkedBrowser;
  await promiseTabState(tab, state);

  info("Restored tab");

  await loadPromise;

  await TabStateFlusher.flush(browser);
  let data = TabState.collect(tab);
  is(data.entries.length, 1, "Should be one entry in session history");
  is(data.entries[0].url, TARGET, "Should be the right session history entry");

  ok(!("__SS_data" in browser), "Temporary restore data should have been cleared");

  // Cleanup.
  await promiseRemoveTab(tab);
});
