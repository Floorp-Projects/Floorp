/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

// Tests that the Web Console CSP messages for two META policies
// are correctly displayed. See Bug 1247459.

"use strict";

const TEST_URI = "data:text/html;charset=utf8,Web Console CSP violation test";
const TEST_VIOLATION = "https://example.com/browser/devtools/client/webconsole/" +
                       "test/mochitest/test-csp-violation.html";
const CSP_VIOLATION_MSG = "Content Security Policy: The page\u2019s settings " +
                          "blocked the loading of a resource at " +
                          "http://some.example.com/test.png (\u201cimg-src\u201d).";

add_task(async function() {
  const hud = await openNewTabAndConsole(TEST_URI);
  hud.ui.clearOutput();

  const onRepeatedMessage = waitForRepeatedMessage(hud, CSP_VIOLATION_MSG, 2);
  await loadDocument(TEST_VIOLATION);
  await onRepeatedMessage;

  ok(true, "Received expected messages");
});
