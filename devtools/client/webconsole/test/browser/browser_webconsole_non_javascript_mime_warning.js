/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

// Tests that <script> loads with non-JavaScript MIME types produce a warning.
// See Bug 1510223.

"use strict";

const TEST_URI =
  "https://example.com/browser/devtools/client/webconsole/" +
  "test/browser/" +
  "test-non-javascript-mime.html";
const MIME_WARNING_MSG =
  "The script from “https://example.com/browser/devtools/client/webconsole/test/browser/test-non-javascript-mime.js” was loaded even though its MIME type (“text/plain”) is not a valid JavaScript MIME type";

add_task(async function() {
  const hud = await openNewTabAndConsole(TEST_URI);
  await waitFor(
    () => findMessage(hud, MIME_WARNING_MSG, ".message.warn"),
    "",
    100
  );
  ok(true, "MIME type warning displayed");
});
