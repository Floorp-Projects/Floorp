/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

// Tests that asm.js AOT can be disabled and debugging of the asm.js code
// is working.

const {
  setupTestRunner,
  asm
} = require("devtools/client/debugger/new/integration-tests");

add_task(function*() {
  setupTestRunner(this);
  yield asm(this);
});
