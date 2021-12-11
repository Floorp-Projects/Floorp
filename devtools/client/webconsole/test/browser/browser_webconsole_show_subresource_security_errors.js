/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

// Ensure non-toplevel security errors are displayed

"use strict";

const TEST_URI =
  "data:text/html;charset=utf-8,Web Console subresource STS warning test";
const TEST_DOC =
  "https://example.com/browser/devtools/client/webconsole/" +
  "test/browser/test-subresource-security-error.html";
const SAMPLE_MSG = "specified a header that could not be parsed successfully.";

add_task(async function() {
  const hud = await openNewTabAndConsole(TEST_URI);
  await clearOutput(hud);
  await navigateTo(TEST_DOC);

  await waitFor(() => findMessage(hud, SAMPLE_MSG, ".message.warn"));

  ok(true, "non-toplevel security warning message was displayed");
});
