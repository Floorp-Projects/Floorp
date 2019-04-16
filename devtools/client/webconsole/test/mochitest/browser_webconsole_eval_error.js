/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

// Tests that throwing uncaught errors while doing console evaluations shows a
// stack in the console.

"use strict";

const TEST_URI = "http://example.com/browser/devtools/client/webconsole/" +
                 "test/mochitest/test-eval-error.html";

add_task(async function() {
  const hud = await openNewTabAndConsole(TEST_URI);

  hud.jsterm.execute("throwErrorObject()");
  await checkMessageStack(hud, "ThrowErrorObject", [6, 1]);

  hud.jsterm.execute("throwValue(40 + 2)");
  await checkMessageStack(hud, "42", [14, 10, 1]);
});
