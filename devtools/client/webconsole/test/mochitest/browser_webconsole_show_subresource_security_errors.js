/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

// Ensure non-toplevel security errors are displayed

"use strict";

const TEST_URI = "data:text/html;charset=utf-8,Web Console subresource STS " +
                 "warning test";
const TEST_DOC = "https://example.com/browser/devtools/client/webconsole/" +
                 "test/mochitest/test-subresource-security-error.html";
const SAMPLE_MSG = "specified a header that could not be parsed successfully.";

add_task(async function() {
  const hud = await openNewTabAndConsole(TEST_URI);
  hud.ui.clearOutput();
  await loadDocument(TEST_DOC);

  await waitFor(() => findMessage(hud, SAMPLE_MSG, ".message.warn"));

  ok(true, "non-toplevel security warning message was displayed");
});
