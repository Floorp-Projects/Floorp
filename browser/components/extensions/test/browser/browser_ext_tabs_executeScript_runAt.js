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

add_task(async function testExecuteScript() {
  let tab = await BrowserTestUtils.openNewForegroundTab(
    gBrowser,
    "about:blank",
    true
  );

  async function background() {
    let tab;

    const BASE =
      "http://mochi.test:8888/browser/browser/components/extensions/test/browser/";
    const URL = BASE + "file_slowed_document.sjs";

    const MAX_TRIES = 10;

    let onUpdatedPromise = (tabId, url, status) => {
      return new Promise(resolve => {
        browser.tabs.onUpdated.addListener(function listener(_, changed, tab) {
          if (tabId == tab.id && changed.status == status && tab.url == url) {
            browser.tabs.onUpdated.removeListener(listener);
            resolve();
          }
        });
      });
    };

    try {
      [tab] = await browser.tabs.query({ active: true, currentWindow: true });

      let success = false;
      for (let tries = 0; !success && tries < MAX_TRIES; tries++) {
        let url = `${URL}?with-iframe&r=${Math.random()}`;

        let loadingPromise = onUpdatedPromise(tab.id, url, "loading");
        let completePromise = onUpdatedPromise(tab.id, url, "complete");

        // TODO: Test allFrames and frameId.

        await browser.tabs.update({ url });
        await loadingPromise;

        let states = await Promise.all(
          [
            // Send the executeScript requests in the reverse order that we expect
            // them to execute in, to avoid them passing only because of timing
            // races.
            browser.tabs.executeScript({
              code: "document.readyState",
              // Testing default `runAt`.
            }),
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
          ].reverse()
        );

        browser.test.log(`Got states: ${states}`);

        // Make sure that none of our scripts executed earlier than expected,
        // regardless of retries.
        browser.test.assertTrue(
          states[1] == "interactive" || states[1] == "complete",
          `document_end state is valid: ${states[1]}`
        );
        browser.test.assertTrue(
          states[2] == "interactive" || states[2] == "complete",
          `document_idle state is valid: ${states[2]}`
        );

        // If we have the earliest valid states for each script, we're done.
        // Otherwise, try again.
        success =
          states[0] == "loading" &&
          states[1] == "interactive" &&
          states[2] == "interactive" &&
          states[3] == "interactive";

        await completePromise;
      }

      browser.test.assertTrue(
        success,
        "Got the earliest expected states at least once"
      );

      browser.test.notifyPass("executeScript-runAt");
    } catch (e) {
      browser.test.fail(`Error: ${e} :: ${e.stack}`);
      browser.test.notifyFail("executeScript-runAt");
    }
  }

  let extension = ExtensionTestUtils.loadExtension({
    manifest: {
      permissions: ["http://mochi.test/", "tabs"],
    },

    background,
  });

  await extension.startup();

  await extension.awaitFinish("executeScript-runAt");

  await extension.unload();

  BrowserTestUtils.removeTab(tab);
});
