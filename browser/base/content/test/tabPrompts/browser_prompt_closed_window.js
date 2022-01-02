/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * Check that if we loop prompts from a closed tab, they don't
 * start showing up as window prompts.
 */
add_task(async function test_closed_tab_doesnt_show_prompt() {
  let newWin = await BrowserTestUtils.openNewBrowserWindow();

  // Get a promise for the initial, in-tab prompt:
  let promptPromise = BrowserTestUtils.promiseAlertDialogOpen();
  await ContentTask.spawn(newWin.gBrowser.selectedBrowser, [], function() {
    // Don't want to block, so use setTimeout with 0 timeout:
    content.setTimeout(
      () =>
        content.eval(
          'let i = 0; while (!prompt("Prompts a lot!") && i++ < 10);'
        ),
      0
    );
  });
  // wait for the first prompt to have appeared:
  await promptPromise;

  // Now close the containing tab, and check for windowed prompts appearing.
  let opened = false;
  let obs = () => {
    opened = true;
  };
  Services.obs.addObserver(obs, "domwindowopened");
  registerCleanupFunction(() =>
    Services.obs.removeObserver(obs, "domwindowopened")
  );
  await BrowserTestUtils.closeWindow(newWin);

  ok(!opened, "Should not have opened a prompt when closing the main window.");
});
