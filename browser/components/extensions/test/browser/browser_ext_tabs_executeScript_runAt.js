/* -*- Mode: indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set sts=2 sw=2 et tw=80: */
"use strict";

/**
 * These tests ensure that the runAt argument to tabs.executeScript delays
 * script execution until the document has reached the correct state.
 *
 * Since tests of this nature are especially race-prone, it relies on a
 * server-JS script to delay the completion of our test page's load cycle long
 * enough for us to attempt to load our scripts in the earlies phase we support.
 *
 * And since we can't actually rely on that timing, it retries any attempts that
 * fail to load as early as expected, but don't load at any illegal time.
 */

add_task(function* testExecuteScript() {
  let tab = yield BrowserTestUtils.openNewForegroundTab(gBrowser, "about:blank", true);

  function background() {
    let tab;

    const BASE = "http://mochi.test:8888/browser/browser/components/extensions/test/browser/";
    const URL = BASE + "file_iframe_document.sjs";

    const MAX_TRIES = 10;
    let tries = 0;

    function again() {
      if (tries++ == MAX_TRIES) {
        return Promise.reject(new Error("Max tries exceeded"));
      }

      let url = `${URL}?r=${Math.random()}`;

      let loadingPromise = new Promise(resolve => {
        browser.tabs.onUpdated.addListener(function listener(tabId, changed, tab_) {
          if (tabId == tab.id && changed.status == "loading" && tab_.url == url) {
            browser.tabs.onUpdated.removeListener(listener);
            resolve();
          }
        });
      });

      // TODO: Test allFrames and frameId.

      return browser.tabs.update({url}).then(() => {
        return loadingPromise;
      }).then(() => {
        return Promise.all([
          // Send the executeScript requests in the reverse order that we expect
          // them to execute in, to avoid them passing only because of timing
          // races.
          browser.tabs.executeScript({
            code: "document.readyState",
            runAt: "document_idle",
          }),
          browser.tabs.executeScript({
            code: "document.readyState",
            runAt: "document_end",
          }),
          browser.tabs.executeScript({
            code: "document.readyState",
            runAt: "document_start",
          }),
        ].reverse());
      }).then(states => {
        browser.test.log(`Got states: ${states}`);

        // Make sure that none of our scripts executed earlier than expected,
        // regardless of retries.
        browser.test.assertTrue(states[1] == "interactive" || states[1] == "complete",
                                `document_end state is valid: ${states[1]}`);
        browser.test.assertTrue(states[2] == "complete",
                                `document_idle state is valid: ${states[2]}`);

        // If we have the earliest valid states for each script, we're done.
        // Otherwise, try again.
        if (states[0] != "loading" || states[1] != "interactive" || states[2] != "complete") {
          return again();
        }
      });
    }

    browser.tabs.query({active: true, currentWindow: true}).then(tabs => {
      tab = tabs[0];

      return again();
    }).then(() => {
      browser.test.notifyPass("executeScript-runAt");
    }).catch(e => {
      browser.test.fail(`Error: ${e} :: ${e.stack}`);
      browser.test.notifyFail("executeScript-runAt");
    });
  }

  let extension = ExtensionTestUtils.loadExtension({
    manifest: {
      "permissions": ["http://mochi.test/", "tabs"],
    },

    background,
  });

  yield extension.startup();

  yield extension.awaitFinish("executeScript-runAt");

  yield extension.unload();

  yield BrowserTestUtils.removeTab(tab);
});
