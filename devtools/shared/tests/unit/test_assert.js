/* -*- js-indent-level: 2; indent-tabs-mode: nil -*- */
/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

// Test DevToolsUtils.assert

ALLOW_CONSOLE_ERRORS = true;

function run_test() {
  // Enable assertions.
  flags.testing = true;

  const { assert } = DevToolsUtils;
  equal(typeof assert, "function");

  try {
    assert(true, "this assertion should not fail");
  } catch (e) {
    // If you catch assertion failures in practice, I will hunt you down. I get
    // email notifications every time it happens.
    ok(false, "Should not get an error for an assertion that should not fail. Got "
       + DevToolsUtils.safeErrorString(e));
  }

  let assertionFailed = false;
  try {
    assert(false, "this assertion should fail");
  } catch (e) {
    ok(e.message.startsWith("Assertion failure:"),
       "Should be an assertion failure error");
    assertionFailed = true;
  }

  ok(assertionFailed,
     "The assertion should have failed, which should throw an error when assertions are enabled.");
}
