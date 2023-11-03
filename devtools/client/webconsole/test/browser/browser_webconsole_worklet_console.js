/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

// Tests that console api usage in worklet show in the console

"use strict";

const TEST_URI =
  "https://example.com/browser/devtools/client/webconsole/" +
  "test/browser/test-console-worklet.html";

add_task(async function () {
  // Allow using SharedArrayBuffer in the test without special HTTP Headers
  await pushPref(
    "dom.postMessage.sharedArrayBuffer.bypassCOOP_COEP.insecure.enabled",
    true
  );

  const hud = await openNewTabAndConsole(TEST_URI);

  await waitFor(() => findConsoleAPIMessage(hud, "string"));
  await waitFor(() => findConsoleAPIMessage(hud, "42"));
  const objectMessage = await waitFor(() =>
    findConsoleAPIMessage(hud, "object")
  );
  ok(
    objectMessage
      .querySelector(".message-body")
      .textContent.includes(`Object { object: true }`),
    "The simple object is logged as expected"
  );
  await waitFor(() => findConsoleAPIMessage(hud, "SharedArrayBuffer"));
  ok(true, "SharedArrayBuffer object is logged");
});
