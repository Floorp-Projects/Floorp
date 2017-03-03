/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

// Tests the breakpoints are hit in various situations.

const {
  setupTestRunner,
  breaking
} = require("devtools/client/debugger/new/integration-tests");

add_task(function*() {
  setupTestRunner(this);
  yield breaking(this);
});
