/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

// Tests that commands run by the user are executed in content space.

"use strict";

const TEST_URI =
  "http://example.com/browser/devtools/client/webconsole/" +
  "test/browser/test-console.html";

add_task(async function () {
  const hud = await openNewTabAndConsole(TEST_URI);
  await clearOutput(hud);

  const onInputMessage = waitForMessageByType(
    hud,
    "window.location.href;",
    ".command"
  );
  const onEvaluationResultMessage = waitForMessageByType(
    hud,
    TEST_URI,
    ".result"
  );
  execute(hud, "window.location.href;");

  let message = await onInputMessage;
  ok(message, "Input message is displayed with the expected class");

  message = await onEvaluationResultMessage;
  ok(message, "EvaluationResult message is displayed with the expected class");
});
