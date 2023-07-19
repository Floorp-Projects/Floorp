/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

// Tests that an uncaught promise rejection inside a Worker or Worklet
// is reported to the tabs' webconsole.

"use strict";

const TEST_URI =
  "https://example.com/browser/devtools/client/webconsole/" +
  "test/browser/test-worker-promise-error.html";

add_task(async function () {
  const hud = await openNewTabAndConsole(TEST_URI);
  await waitFor(() =>
    findErrorMessage(hud, "uncaught exception: worker-error")
  );

  await waitFor(() =>
    findErrorMessage(hud, "uncaught exception: worklet-error")
  );

  ok(true, "received error messages");
});
