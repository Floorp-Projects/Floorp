/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const { PromiseTestUtils } = ChromeUtils.importESModule(
  "resource://testing-common/PromiseTestUtils.sys.mjs"
);

/**
 * Check that if we're using a window-modal prompt,
 * the next synchronous window-internal modal prompt aborts rather than
 * leaving us in a deadlock about how to enter modal state.
 */
add_task(async function test_check_multiple_prompts() {
  await SpecialPowers.pushPrefEnv({
    set: [["prompts.windowPromptSubDialog", true]],
  });
  let container = document.getElementById("window-modal-dialog");
  let dialogPromise = BrowserTestUtils.promiseAlertDialogOpen();

  let firstDialogClosedPromise = new Promise(resolve => {
    // Avoid blocking the test on the (sync) alert by sticking it in a timeout:
    setTimeout(() => {
      Services.prompt.alertBC(
        window.browsingContext,
        Ci.nsIPrompt.MODAL_TYPE_WINDOW,
        "Some title",
        "some message"
      );
      resolve();
    }, 0);
  });
  let dialogWin = await dialogPromise;

  // Check circumstances of opening.
  ok(
    !dialogWin.docShell.chromeEventHandler,
    "Should not have embedded the dialog."
  );

  PromiseTestUtils.expectUncaughtRejection(/could not be shown/);
  let rv = Services.prompt.confirm(
    window,
    "I should not appear",
    "because another prompt was open"
  );
  is(rv, false, "Prompt should have been canceled.");

  info("Accepting dialog");
  dialogWin.document.querySelector("dialog").acceptDialog();

  await firstDialogClosedPromise;

  await BrowserTestUtils.waitForMutationCondition(
    container,
    { childList: true, attributes: true },
    () => !container.hasChildNodes() && !container.open
  );
});
