/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

// Tests that importScripts loads inside a worker with a non-JavaScript
// MIME types produce an error and fail.
// See Bug 1514680.

"use strict";

const TEST_URI =
  "http://example.com/browser/devtools/client/webconsole/" +
  "test/mochitest/" +
  "test-non-javascript-mime-worker.html";
const MIME_ERROR_MSG =
  "Loading script from “http://example.com/browser/devtools/client/webconsole/test/mochitest/test-non-javascript-mime.js” with importScripts() was blocked because of a disallowed MIME type (“text/plain”).";

add_task(async function() {
  const hud = await openNewTabAndConsole(TEST_URI);
  await waitFor(
    () => findMessage(hud, MIME_ERROR_MSG, ".message.error"),
    "",
    100
  );
  ok(true, "MIME type error displayed");
});
