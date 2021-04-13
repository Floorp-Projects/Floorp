"use strict";

// Tests that a second attempt to close a window while blocked on a
// beforeunload confirmation ignores the beforeunload listener and
// unblocks the original close call.

const CONTENT_PROMPT_SUBDIALOG = Services.prefs.getBoolPref(
  "prompts.contentPromptSubDialog",
  false
);

const DIALOG_TOPIC = CONTENT_PROMPT_SUBDIALOG
  ? "common-dialog-loaded"
  : "tabmodal-dialog-loaded";

add_task(async function() {
  await SpecialPowers.pushPrefEnv({
    set: [["dom.require_user_interaction_for_beforeunload", false]],
  });

  let win = await BrowserTestUtils.openNewBrowserWindow();

  let browser = win.gBrowser.selectedBrowser;
  BrowserTestUtils.loadURI(browser, "http://example.com/");
  await BrowserTestUtils.browserLoaded(browser, false, "http://example.com/");

  await SpecialPowers.spawn(browser, [], () => {
    // eslint-disable-next-line mozilla/balanced-listeners
    content.addEventListener("beforeunload", event => {
      event.preventDefault();
    });
  });

  let confirmationShown = false;

  BrowserUtils.promiseObserved(DIALOG_TOPIC).then(() => {
    confirmationShown = true;
    win.close();
  });

  win.close();
  ok(confirmationShown, "Before unload confirmation should have been shown");
  ok(win.closed, "Window should have been closed after second close() call");
});
