/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

const {
  callStack,
  setupTestRunner
} = require("devtools/client/debugger/new/integration-tests");

add_task(function*() {
  setupTestRunner(this);
  yield callStack.test1(this);
});

add_task(function*() {
  setupTestRunner(this);
  yield callStack.test2(this);
});
