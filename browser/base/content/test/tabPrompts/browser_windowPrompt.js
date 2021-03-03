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
  let dialogPromise = BrowserTestUtils.promiseAlertDialogOpen(null);
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
  is(
    window.getComputedStyle(document.body).getPropertyValue("-moz-user-input"),
    "none",
    "Browser window should be inert."
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

  // Check we cleaned up:
  is(
    window.getComputedStyle(document.body).getPropertyValue("-moz-user-input"),
    "auto",
    "Browser window should no longer be inert."
  );
  for (let menu of document.querySelectorAll("menubar > menu")) {
    ok(!menu.disabled, `Menu ${menu.id} should not be disabled anymore.`);
  }
});
