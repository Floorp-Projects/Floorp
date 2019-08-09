/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Check instanceof correctness. See Bug 599940.
const TEST_URI = "data:text/html,Test <code>instanceof</code> evaluation";

add_task(async function() {
  const hud = await openNewTabAndConsole(TEST_URI);

  let message = await executeAndWaitForMessage(
    hud,
    "[] instanceof Array",
    "true",
    ".result"
  );
  ok(message, "`instanceof Array` is correct");

  message = await executeAndWaitForMessage(
    hud,
    "({}) instanceof Object",
    "true",
    ".result"
  );
  ok(message, "`instanceof Object` is correct");

  message = await executeAndWaitForMessage(
    hud,
    "({}) instanceof Array",
    "false",
    ".result"
  );
  ok(message, "`instanceof Array` has expected result");
});
