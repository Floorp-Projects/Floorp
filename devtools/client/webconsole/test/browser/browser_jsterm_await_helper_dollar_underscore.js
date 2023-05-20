/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

// Test that $_ works as expected with top-level await expressions.

"use strict";
requestLongerTimeout(2);

const TEST_URI =
  "data:text/html;charset=utf-8,<!DOCTYPE html>top-level await + $_";

add_task(async function () {
  // Enable await mapping.
  await pushPref("devtools.debugger.features.map-await-expression", true);
  const hud = await openNewTabAndConsole(TEST_URI);

  info("Evaluate a simple expression to populate $_");
  await executeAndWaitForResultMessage(hud, `1 + 1`, `2`);

  await executeAndWaitForResultMessage(hud, `$_ + 1`, `3`);
  ok(true, "$_ works as expected");

  info(
    "Check that $_ does not get replaced until the top-level await is resolved"
  );
  const onAwaitResultMessage = executeAndWaitForResultMessage(
    hud,
    `await new Promise(res => setTimeout(() => res([1,2,3, $_]), 1000))`,
    `Array(4) [ 1, 2, 3, 4 ]`
  );

  await executeAndWaitForResultMessage(hud, `$_ + 1`, `4`);
  ok(true, "$_ was not impacted by the top-level await input");

  await onAwaitResultMessage;
  ok(true, "the top-level await result can use $_ in its returned value");

  await executeAndWaitForResultMessage(
    hud,
    `await new Promise(res => setTimeout(() => res([...$_, 5]), 1000))`,
    `Array(5) [ 1, 2, 3, 4, 5 ]`
  );
  ok(true, "$_ is assigned with the result of the top-level await");

  info("Check that awaiting for a rejecting promise does not re-assign $_");
  await executeAndWaitForErrorMessage(
    hud,
    `x = await new Promise((resolve,reject) =>
      setTimeout(() => reject("await-" + "rej"), 500))`,
    `await-rej`
  );

  await executeAndWaitForResultMessage(hud, `$_`, `Array(5) [ 1, 2, 3, 4, 5 ]`);
  ok(true, "$_ wasn't re-assigned");

  info("Check that $_ gets the value of the last resolved await expression");
  const delays = [2000, 1000, 4000, 3000];
  const inputs = delays.map(
    delay => `await new Promise(
    r => setTimeout(() => r("await-concurrent-" + ${delay}), ${delay}))`
  );

  // Let's wait for the message that should be displayed last.
  const onMessage = waitForMessageByType(
    hud,
    "await-concurrent-4000",
    ".result"
  );
  for (const input of inputs) {
    execute(hud, input);
  }
  await onMessage;

  await executeAndWaitForResultMessage(
    hud,
    `"result: " + $_`,
    `"result: await-concurrent-4000"`
  );
  ok(
    true,
    "$_ was replaced with the last resolving top-level await evaluation result"
  );
});
