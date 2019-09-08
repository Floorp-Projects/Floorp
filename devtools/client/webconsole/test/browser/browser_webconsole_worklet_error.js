/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

// Tests that syntax errors in worklet scripts show in the console and that
// throwing uncaught errors and primitive values in worklets shows a stack.

"use strict";

const TEST_URI =
  "https://example.com/browser/devtools/client/webconsole/" +
  "test/browser/test-error-worklet.html";

add_task(async function() {
  await SpecialPowers.pushPrefEnv({
    set: [["dom.audioworklet.enabled", true], ["dom.worklet.enabled", true]],
  });

  const hud = await openNewTabAndConsole(TEST_URI);

  await waitFor(() =>
    findMessage(hud, "SyntaxError: duplicate formal argument")
  );
  ok(true, "Received expected SyntaxError");

  await checkMessageStack(hud, "addModule", [18, 21]);
  await checkMessageStack(hud, "process", [7, 12]);
});
