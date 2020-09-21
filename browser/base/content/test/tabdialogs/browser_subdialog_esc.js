/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const TEST_ROOT_CHROME = getRootDirectory(gTestPath);
const TEST_DIALOG_PATH = TEST_ROOT_CHROME + "subdialog.xhtml";

const WEB_ROOT = TEST_ROOT_CHROME.replace(
  "chrome://mochitests/content",
  "http://example.com"
);
const TEST_LOAD_PAGE = WEB_ROOT + "loadDelayedReply.sjs";

/**
 * Tests that ESC on a SubDialog does not cancel ongoing loads in the parent.
 */
add_task(async function test_subdialog_esc_does_not_cancel_load() {
  await BrowserTestUtils.withNewTab("http://example.com", async function(
    browser
  ) {
    // Start loading a page
    let loadStartedPromise = BrowserTestUtils.loadURI(browser, TEST_LOAD_PAGE);
    let loadedPromise = BrowserTestUtils.browserLoaded(browser);
    await loadStartedPromise;

    // Open a dialog
    let dialogBox = gBrowser.getTabDialogBox(browser);
    let dialogClose = dialogBox.open(TEST_DIALOG_PATH, {
      keepOpenSameOriginNav: true,
    });

    let dialogs = dialogBox._dialogManager._dialogs;

    is(dialogs.length, 1, "Dialog manager has a dialog.");

    info("Waiting for dialogs to open.");
    await dialogs[0]._dialogReady;

    // Close the dialog with esc key
    EventUtils.synthesizeKey("KEY_Escape");

    info("Waiting for dialog to close.");
    await dialogClose;

    info("Triggering load complete");
    fetch(TEST_LOAD_PAGE, {
      method: "POST",
    });

    // Load must complete
    info("Waiting for load to complete");
    await loadedPromise;
    ok(true, "Load completed");
  });
});
