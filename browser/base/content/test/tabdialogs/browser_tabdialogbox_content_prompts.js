/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const CONTENT_PROMPT_PREF = "prompts.contentPromptSubDialog";
const TEST_ROOT_CHROME = getRootDirectory(gTestPath);
const TEST_DIALOG_PATH = TEST_ROOT_CHROME + "subdialog.xhtml";
const TEST_URL = "data:text/html,<body onload='alert(1)'>";

// Setup.
add_task(async function setup() {
  await SpecialPowers.pushPrefEnv({
    set: [[CONTENT_PROMPT_PREF, true]],
  });
});

/**
 * Test that a manager for content prompts is added to tab dialog box.
 */
add_task(async function test_tabdialog_content_prompts() {
  await BrowserTestUtils.withNewTab("http://example.com", async function(
    browser
  ) {
    info("Open a tab prompt.");
    let dialogBox = gBrowser.getTabDialogBox(browser);
    dialogBox.open(TEST_DIALOG_PATH);

    info("Check the content prompt dialog is only created when needed.");
    let contentPromptDialog = document.querySelector(".content-prompt-dialog");
    ok(!contentPromptDialog, "Content prompt dialog should not be created.");

    info("Open a content prompt");
    dialogBox.open(TEST_DIALOG_PATH, {
      modalType: Ci.nsIPrompt.MODAL_TYPE_CONTENT,
    });

    contentPromptDialog = document.querySelector(".content-prompt-dialog");
    ok(contentPromptDialog, "Content prompt dialog should be created.");
    let contentPromptManager = dialogBox.getContentDialogManager();

    is(
      contentPromptManager._dialogs.length,
      1,
      "Content prompt manager should have 1 dialog box."
    );
  });
});

/**
 * Test that title text is shown in tabmodal alert/confirm/prompt dialogs.
 */
add_task(async function test_tabdialog_show_title() {
  let dialogShown = BrowserTestUtils.waitForEvent(
    gBrowser,
    "DOMWillOpenModalDialog"
  );

  await BrowserTestUtils.withNewTab(TEST_URL, async function(browser) {
    info("Waiting for dialog to open.");
    await dialogShown;

    info("Check the title is visible.");
    let dialogBox = gBrowser.getTabDialogBox(browser);
    let contentPromptManager = dialogBox.getContentDialogManager();
    let dialog = contentPromptManager._dialogs[0];

    info("Waiting for dialog frame to be ready.");
    await dialog._dialogReady;

    let dialogDoc = dialog._frame.contentWindow.document;
    let infoTitle = dialogDoc.querySelector("#infoTitle");

    ok(BrowserTestUtils.is_visible(infoTitle), "Title text is visible");
  });
});
