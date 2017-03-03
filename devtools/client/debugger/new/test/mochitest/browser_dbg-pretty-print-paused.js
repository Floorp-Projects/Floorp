/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

// Tests pretty-printing a source that is currently paused.

const {
  setupTestRunner,
  prettyPrintPaused
} = require("devtools/client/debugger/new/integration-tests");

add_task(function*() {
  setupTestRunner(this);
  yield prettyPrintPaused(this);
});
