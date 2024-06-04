/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

// This test verifies that focus is handled correctly when a
// tab is dragged out to a new window, by checking that the
// copy and select all commands are enabled properly.
add_task(async function () {
  let tab1 = await BrowserTestUtils.openNewForegroundTab(
    gBrowser,
    "https://www.example.com"
  );
  let tab2 = await BrowserTestUtils.openNewForegroundTab(
    gBrowser,
    "https://www.example.com"
  );

  let delayedStartupPromise = BrowserTestUtils.waitForNewWindow();
  let win = gBrowser.replaceTabWithWindow(tab2);
  await delayedStartupPromise;

  let copyCommand = win.document.getElementById("cmd_copy");
  info("Waiting for copy to be enabled");
  await BrowserTestUtils.waitForMutationCondition(
    copyCommand,
    { attributes: true },
    () => {
      return !copyCommand.hasAttribute("disabled");
    }
  );

  ok(
    !win.document.getElementById("cmd_copy").hasAttribute("disabled"),
    "copy is enabled"
  );
  ok(
    !win.document.getElementById("cmd_selectAll").hasAttribute("disabled"),
    "select all is enabled"
  );

  await BrowserTestUtils.closeWindow(win);
  await BrowserTestUtils.removeTab(tab1);
});
