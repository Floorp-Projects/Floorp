/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const CONTENT_PROMPT_PREF = "prompts.contentPromptSubDialog";
const TEST_ROOT_CHROME = getRootDirectory(gTestPath);
const TEST_DIALOG_PATH = TEST_ROOT_CHROME + "subdialog.xhtml";

/**
 * Test that a manager for content prompts is added to tab dialog box.
 */
add_task(async function test_tabdialog_content_prompts() {
  await BrowserTestUtils.withNewTab("http://example.com", async function(
    browser
  ) {
    await SpecialPowers.pushPrefEnv({
      set: [[CONTENT_PROMPT_PREF, true]],
    });

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
