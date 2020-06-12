/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

// Tests that any output created from calls to the console API comes before the
// echoed JavaScript.

"use strict";

const TEST_URI =
  "http://example.com/browser/devtools/client/webconsole/" +
  "test/browser/test-console.html";

add_task(async function() {
  const hud = await openNewTabAndConsole(TEST_URI);

  const evaluationResultMessage = await executeAndWaitForMessage(
    hud,
    "console.log('foo', 'bar')",
    "undefined",
    ".result"
  );
  const commandMessage = findMessage(hud, "console.log");
  const logMessage = findMessage(hud, "foo bar");

  is(
    commandMessage.nextElementSibling,
    logMessage,
    "console.log() is followed by 'foo' 'bar'"
  );

  is(
    logMessage.nextElementSibling,
    evaluationResultMessage.node,
    "'foo bar' is followed by undefined"
  );
});
