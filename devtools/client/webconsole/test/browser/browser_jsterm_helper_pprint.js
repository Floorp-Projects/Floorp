/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const TEST_URI = "data:text/html,Test <code>pprint()</code> jsterm helper";

add_task(async function() {
  const hud = await openNewTabAndConsole(TEST_URI);

  let message = await executeAndWaitForMessage(
    hud,
    "pprint({b:2, a:1})",
    `"  b: 2\n  a: 1"`,
    ".result"
  );
  ok(message, "`pprint()` worked");

  // check that pprint(window) does not throw (see Bug 608358).
  message = await executeAndWaitForMessage(
    hud,
    "pprint(window)",
    `window:`,
    ".result"
  );
  ok(message, "`pprint(window)` worked");

  // check that calling pprint with a string does not throw (See Bug 614561).
  message = await executeAndWaitForMessage(
    hud,
    "pprint('hi')",
    `"  0: \\"h\\"\n  1: \\"i\\""`,
    ".result"
  );
  ok(message, "`pprint('hi')` worked");

  // check that pprint(function) shows function source (See Bug 618344).
  message = await executeAndWaitForMessage(
    hud,
    "pprint(function() { var someCanaryValue = 42; })",
    `"function() { var someCanaryValue = 42; }`,
    ".result"
  );
  ok(message, "`pprint(function)` shows function source");
});
