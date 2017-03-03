/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

const {
  setupTestRunner,
  returnvalues
} = require("devtools/client/debugger/new/integration-tests");

add_task(function*() {
  setupTestRunner(this);
  yield returnvalues(this);
});
