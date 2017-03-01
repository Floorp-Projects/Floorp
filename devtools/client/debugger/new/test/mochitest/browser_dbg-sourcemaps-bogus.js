/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

// Test that an error while loading a sourcemap does not break
// debugging.

const {
  setupTestRunner,
  sourceMapsBogus
} = require("devtools/client/debugger/new/integration-tests");

add_task(function*() {
  setupTestRunner(this);
  yield sourceMapsBogus(this);
});
