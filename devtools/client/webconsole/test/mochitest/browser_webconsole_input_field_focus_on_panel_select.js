/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

// Test that the JS input field is focused when the user switches back to the
// web console from other tools, see bug 891581.

"use strict";

const TEST_URI = "data:text/html;charset=utf8,<p>Test console input focus";

add_task(async function() {
  const hud = await openNewTabAndConsole(TEST_URI);

  info("Focus after console is opened");
  ok(isInputFocused(hud), "input is focused after console is opened");

  const filterInput = getFilterInput(hud);
  filterInput.focus();
  ok(hasFocus(filterInput), "filter input should be focused");

  is(isInputFocused(hud), false, "input node is not focused anymore");

  info("Go to the inspector panel");
  await openInspector();

  info("Go back to the console");
  await openConsole();

  ok(
    isInputFocused(hud),
    "input is focused when coming from a different panel"
  );
});
