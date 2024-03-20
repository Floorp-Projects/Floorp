/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const kTestPage = "https://example.org/browser/dom/html/test/bug436200.html";

async function run_test(shouldShowPrompt, msg) {
  let promptShown = false;

  function commonDialogObserver(subject) {
    let dialog = subject.Dialog;
    promptShown = true;
    dialog.ui.button0.click();
  }
  Services.obs.addObserver(commonDialogObserver, "common-dialog-loaded");

  let tab = await BrowserTestUtils.openNewForegroundTab(gBrowser, kTestPage);

  await SpecialPowers.spawn(gBrowser.selectedBrowser, [], async function () {
    let form = content.document.getElementById("test_form");
    form.submit();
  });
  Services.obs.removeObserver(commonDialogObserver, "common-dialog-loaded");

  is(promptShown, shouldShowPrompt, msg);
  BrowserTestUtils.removeTab(tab);
}

add_task(async function test_prompt() {
  await run_test(true, "Should show prompt");
});

add_task(async function test_noprompt() {
  await SpecialPowers.pushPrefEnv({
    set: [["security.warn_submit_secure_to_insecure", false]],
  });
  await run_test(false, "Should not show prompt");
  await SpecialPowers.popPrefEnv();
});

add_task(async function test_prompt_modal() {
  await SpecialPowers.pushPrefEnv({
    set: [
      [
        "prompts.modalType.insecureFormSubmit",
        Services.prompt.MODAL_TYPE_WINDOW,
      ],
    ],
  });
  await run_test(true, "Should show prompt");
  await SpecialPowers.popPrefEnv();
});
