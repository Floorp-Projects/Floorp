/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

// Tests that the code with > 5 lines in multiline editor mode
// is collapsible
// Check Bug 1578212 for more info

"use strict";
const { ELLIPSIS } = require("resource://devtools/shared/l10n.js");

const SMALL_EXPRESSION = `function fib(n) {
  if (n <= 1)
    return 1;
  return fib(n-1) + fib(n-2);
}`;

const LONG_EXPRESSION = `${SMALL_EXPRESSION}
fib(3);`;

add_task(async function () {
  const hud = await openNewTabAndConsole(
    "data:text/html,<!DOCTYPE html><meta charset=utf8>Test multi-line commands expandability"
  );
  info("Test that we don't slice messages with <= 5 lines");
  const message = await executeAndWaitForMessageByType(
    hud,
    SMALL_EXPRESSION,
    "function fib",
    ".command"
  );

  is(
    message.node.querySelector(".collapse-button"),
    null,
    "Collapse button does not exist"
  );

  info("Test messages with > 5 lines are sliced");

  const messageExp = await executeAndWaitForMessageByType(
    hud,
    LONG_EXPRESSION,
    "function fib",
    ".command"
  );

  const toggleArrow = messageExp.node.querySelector(".collapse-button");
  ok(toggleArrow, "Collapse button exists");
  // Check for elipsis
  ok(messageExp.node.innerText.includes(ELLIPSIS), "Has ellipsis");

  info("Test clicking the button expands/collapses the message");

  const isOpen = node2 => node2.classList.contains("open");

  toggleArrow.click(); // expand
  await waitFor(() => isOpen(messageExp.node) === true);

  ok(
    !messageExp.node.innerText.includes(ELLIPSIS),
    "Opened message doesn't have ellipsis"
  );
  is(
    messageExp.node.innerText.trim().split("\n").length,
    LONG_EXPRESSION.split("\n").length,
    "Expanded code has same number of lines as original"
  );

  toggleArrow.click(); // expand
  await waitFor(() => isOpen(messageExp.node) === false);

  is(
    messageExp.node.innerText.trim().split("\n").length,
    5,
    "Code is truncated & only 5 lines shown"
  );
});
