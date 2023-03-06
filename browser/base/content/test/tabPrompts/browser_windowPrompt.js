/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * Check that the in-window modal dialogs work correctly.
 */
add_task(async function test_check_window_modal_prompt_service() {
  await SpecialPowers.pushPrefEnv({
    set: [["prompts.windowPromptSubDialog", true]],
  });
  let dialogPromise = BrowserTestUtils.promiseAlertDialogOpen();
  // Avoid blocking the test on the (sync) alert by sticking it in a timeout:
  setTimeout(
    () => Services.prompt.alert(window, "Some title", "some message"),
    0
  );
  let dialogWin = await dialogPromise;

  // Check dialog content:
  is(
    dialogWin.document.getElementById("infoTitle").textContent,
    "Some title",
    "Title should be correct."
  );
  ok(
    !dialogWin.document.getElementById("infoTitle").hidden,
    "Title should be shown."
  );
  is(
    dialogWin.document.getElementById("infoBody").textContent,
    "some message",
    "Body text should be correct."
  );

  // Check circumstances of opening.
  ok(
    dialogWin?.docShell?.chromeEventHandler,
    "Should have embedded the dialog."
  );
  for (let menu of document.querySelectorAll("menubar > menu")) {
    ok(menu.disabled, `Menu ${menu.id} should be disabled.`);
  }

  let container = dialogWin.docShell.chromeEventHandler.closest("dialog");
  let closedPromise = BrowserTestUtils.waitForMutationCondition(
    container,
    { childList: true, attributes: true },
    () => !container.hasChildNodes() && !container.open
  );

  EventUtils.sendKey("ESCAPE");
  await closedPromise;

  await BrowserTestUtils.waitForMutationCondition(
    document.querySelector("menubar > menu"),
    { attributes: true },
    () => !document.querySelector("menubar > menu").disabled
  );

  // Check we cleaned up:
  for (let menu of document.querySelectorAll("menubar > menu")) {
    ok(!menu.disabled, `Menu ${menu.id} should not be disabled anymore.`);
  }
});

/**
 * Check that the dialog's own closing methods being invoked don't break things.
 */
add_task(async function test_check_window_modal_prompt_service() {
  await SpecialPowers.pushPrefEnv({
    set: [["prompts.windowPromptSubDialog", true]],
  });
  let dialogPromise = BrowserTestUtils.promiseAlertDialogOpen();
  // Avoid blocking the test on the (sync) alert by sticking it in a timeout:
  setTimeout(
    () => Services.prompt.alert(window, "Some title", "some message"),
    0
  );
  let dialogWin = await dialogPromise;

  // Check circumstances of opening.
  ok(
    dialogWin?.docShell?.chromeEventHandler,
    "Should have embedded the dialog."
  );

  let container = dialogWin.docShell.chromeEventHandler.closest("dialog");
  let closedPromise = BrowserTestUtils.waitForMutationCondition(
    container,
    { childList: true, attributes: true },
    () => !container.hasChildNodes() && !container.open
  );

  // This can also be invoked by the user if the escape key is handled
  // outside of our embedded dialog.
  container.close();
  await closedPromise;

  // Check we cleaned up:
  for (let menu of document.querySelectorAll("menubar > menu")) {
    ok(!menu.disabled, `Menu ${menu.id} should not be disabled anymore.`);
  }
});

add_task(async function test_check_multiple_prompts() {
  await SpecialPowers.pushPrefEnv({
    set: [["prompts.windowPromptSubDialog", true]],
  });
  let container = document.getElementById("window-modal-dialog");
  let dialogPromise = BrowserTestUtils.promiseAlertDialogOpen();

  let firstDialogClosedPromise = new Promise(resolve => {
    // Avoid blocking the test on the (sync) alert by sticking it in a timeout:
    setTimeout(() => {
      Services.prompt.alert(window, "Some title", "some message");
      resolve();
    }, 0);
  });
  let dialogWin = await dialogPromise;

  // Check circumstances of opening.
  ok(
    dialogWin?.docShell?.chromeEventHandler,
    "Should have embedded the dialog."
  );
  is(container.childElementCount, 1, "Should only have 1 dialog in the DOM.");

  let secondDialogClosedPromise = new Promise(resolve => {
    // Avoid blocking the test on the (sync) alert by sticking it in a timeout:
    setTimeout(() => {
      Services.prompt.alert(window, "Another title", "another message");
      resolve();
    }, 0);
  });

  dialogPromise = BrowserTestUtils.promiseAlertDialogOpen();

  info("Accepting dialog");
  dialogWin.document.querySelector("dialog").acceptDialog();

  let oldWin = dialogWin;

  info("Second dialog should automatically come up.");
  dialogWin = await dialogPromise;

  isnot(oldWin, dialogWin, "Opened a new dialog.");
  ok(container.open, "Dialog should be open.");

  info("Now close the second dialog.");
  dialogWin.document.querySelector("dialog").acceptDialog();

  await firstDialogClosedPromise;
  await secondDialogClosedPromise;

  await BrowserTestUtils.waitForMutationCondition(
    container,
    { childList: true, attributes: true },
    () => !container.hasChildNodes() && !container.open
  );
  // Check we cleaned up:
  for (let menu of document.querySelectorAll("menubar > menu")) {
    ok(!menu.disabled, `Menu ${menu.id} should not be disabled anymore.`);
  }
});

/**
 * Check that the in-window modal dialogs un-minimizes windows when necessary.
 */
add_task(async function test_check_minimize_response() {
  // Window minimization doesn't necessarily work on Linux...
  if (AppConstants.platform == "linux") {
    return;
  }
  await SpecialPowers.pushPrefEnv({
    set: [["prompts.windowPromptSubDialog", true]],
  });

  let promiseSizeModeChange = BrowserTestUtils.waitForEvent(
    window,
    "sizemodechange"
  );
  window.minimize();
  await promiseSizeModeChange;
  is(window.windowState, window.STATE_MINIMIZED, "Should be minimized.");

  promiseSizeModeChange = BrowserTestUtils.waitForEvent(
    window,
    "sizemodechange"
  );
  let dialogPromise = BrowserTestUtils.promiseAlertDialogOpen();
  // Use an async alert to avoid blocking.
  Services.prompt.asyncAlert(
    window.browsingContext,
    Ci.nsIPrompt.MODAL_TYPE_INTERNAL_WINDOW,
    "Some title",
    "some message"
  );
  let dialogWin = await dialogPromise;
  await promiseSizeModeChange;

  isnot(
    window.windowState,
    window.STATE_MINIMIZED,
    "Should no longer be minimized."
  );

  // Check dialog content:
  is(
    dialogWin.document.getElementById("infoTitle").textContent,
    "Some title",
    "Title should be correct."
  );

  let container = dialogWin.docShell.chromeEventHandler.closest("dialog");
  let closedPromise = BrowserTestUtils.waitForMutationCondition(
    container,
    { childList: true, attributes: true },
    () => !container.hasChildNodes() && !container.open
  );

  EventUtils.sendKey("ESCAPE");
  await closedPromise;

  await BrowserTestUtils.waitForMutationCondition(
    document.querySelector("menubar > menu"),
    { attributes: true },
    () => !document.querySelector("menubar > menu").disabled
  );
});

/**
 * Tests that we get a closed callback even when closing the prompt before the
 * underlying SubDialog has fully opened.
 */
add_task(async function test_closed_callback() {
  await SpecialPowers.pushPrefEnv({
    set: [["prompts.windowPromptSubDialog", true]],
  });

  let promptClosedPromise = Services.prompt.asyncAlert(
    window.browsingContext,
    Services.prompt.MODAL_TYPE_INTERNAL_WINDOW,
    "Hello",
    "Hello, World!"
  );

  let dialog = gDialogBox._dialog;
  ok(dialog, "gDialogBox should have a dialog");

  // Directly close the dialog without waiting for it to initialize.
  dialog.close();

  info("Waiting for prompt close");
  await promptClosedPromise;

  ok(!gDialogBox._dialog, "gDialogBox should no longer have a dialog");
});
