/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

// Tests that importScripts loads inside a worker with a non-JavaScript
// MIME types produce an error and fail.
// See Bug 1514680.
// Also tests that `new Worker` with a non-JS MIME type fails. (Bug 1523706)

"use strict";

const TEST_URI =
  "https://example.com/browser/devtools/client/webconsole/" +
  "test/browser/" +
  "test-non-javascript-mime-worker.html";

const JS_URI =
  "https://example.com/browser/devtools/client/webconsole/test/browser/test-non-javascript-mime.js";
const MIME_ERROR_MSG1 = `Loading Worker from “${JS_URI}” was blocked because of a disallowed MIME type (“text/plain”).`;
const MIME_ERROR_MSG2 = `Loading script from “${JS_URI}” with importScripts() was blocked because of a disallowed MIME type (“text/plain”).`;

add_task(async function () {
  const hud = await openNewTabAndConsole(TEST_URI);
  await waitFor(() => findErrorMessage(hud, MIME_ERROR_MSG1), "", 100);
  await waitFor(() => findErrorMessage(hud, MIME_ERROR_MSG2), "", 100);
  ok(true, "MIME type error displayed");
});
