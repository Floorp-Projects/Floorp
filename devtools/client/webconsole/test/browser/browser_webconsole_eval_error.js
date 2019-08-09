/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

// Tests that throwing uncaught errors while doing console evaluations shows a
// stack in the console.

"use strict";

const TEST_URI =
  "http://example.com/browser/devtools/client/webconsole/" +
  "test/browser/test-eval-error.html";

add_task(async function() {
  const hud = await openNewTabAndConsole(TEST_URI);

  execute(hud, "throwErrorObject()");
  await checkMessageStack(hud, "ThrowErrorObject", [6, 1]);

  execute(hud, "throwValue(40 + 2)");
  await checkMessageStack(hud, "42", [14, 10, 1]);

  execute(
    hud,
    `
    a = () => {throw "bloop"};
    b =  () => a();
    c =  () => b();
    d =  () => c();
    d();
  `
  );
  await checkMessageStack(hud, "Error: bloop", [2, 3, 4, 5, 6]);

  execute(hud, `1 + @`);
  const messageNode = await waitFor(() =>
    findMessage(hud, "illegal character")
  );
  is(
    messageNode.querySelector(".frames"),
    null,
    "There's no stacktrace for a SyntaxError evaluation"
  );
});
