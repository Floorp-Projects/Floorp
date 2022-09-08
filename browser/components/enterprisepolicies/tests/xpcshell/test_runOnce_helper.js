/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

let { runOnce } = ChromeUtils.importESModule(
  "resource:///modules/policies/Policies.sys.mjs"
);

let runCount = 0;
function callback() {
  runCount++;
}

add_task(async function test_runonce_helper() {
  runOnce("test_action", callback);
  equal(runCount, 1, "Callback ran for the first time.");

  runOnce("test_action", callback);
  equal(runCount, 1, "Callback didn't run again.");
});
