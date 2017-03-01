/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

// Tests that keyboard navigation into and out of debugger code editor

const {
  setupTestRunner,
  keyboardNavigation
} = require("devtools/client/debugger/new/integration-tests");

add_task(function*() {
  setupTestRunner(this);
  yield keyboardNavigation(this);
});
