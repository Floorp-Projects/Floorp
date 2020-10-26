/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * Most commands don't make sense in customize mode. Check that they're
 * disabled, so shortcuts can't activate them either. Also check that
 * some basic commands (close tab/window, quit, new tab, new window)
 * remain functional.
 */
add_task(async function test_disable_commands() {
  let disabledCommands = ["cmd_print", "Browser:SavePage", "Browser:SendLink"];
  let enabledCommands = [
    "cmd_newNavigatorTab",
    "cmd_newNavigator",
    "cmd_quitApplication",
    "cmd_close",
    "cmd_closeWindow",
  ];

  function checkDisabled() {
    for (let cmd of disabledCommands) {
      is(
        document.getElementById(cmd).getAttribute("disabled"),
        "true",
        `Command ${cmd} should be disabled`
      );
    }
    for (let cmd of enabledCommands) {
      ok(
        !document.getElementById(cmd).hasAttribute("disabled"),
        `Command ${cmd} should NOT be disabled`
      );
    }
  }
  await startCustomizing();

  checkDisabled();

  // Do a reset just for fun, making sure we don't accidentally
  // break things:
  await gCustomizeMode.reset();

  checkDisabled();

  await endCustomizing();
  for (let cmd of disabledCommands.concat(enabledCommands)) {
    ok(
      !document.getElementById(cmd).hasAttribute("disabled"),
      `Command ${cmd} should NOT be disabled after customize mode`
    );
  }
});

/**
 * When buttons are connected to a command, they should not get
 * disabled just because we move them.
 */
add_task(async function test_dont_disable_when_moving() {
  let button = gNavToolbox.palette.querySelector("#print-button");
  ok(button.hasAttribute("command"), "Button should have a command attribute.");
  await startCustomizing();
  CustomizableUI.addWidgetToArea("print-button", "nav-bar");
  await endCustomizing();
  ok(
    !button.hasAttribute("disabled"),
    "Should not have disabled attribute after adding the button."
  );
  ok(
    button.hasAttribute("command"),
    "Button should still have a command attribute."
  );

  await startCustomizing();
  await gCustomizeMode.reset();
  await endCustomizing();
  ok(
    !button.hasAttribute("disabled"),
    "Should not have disabled attribute when resetting in customize mode"
  );
  ok(
    button.hasAttribute("command"),
    "Button should still have a command attribute."
  );
});
