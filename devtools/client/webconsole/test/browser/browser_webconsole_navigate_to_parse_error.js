/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

// Tests that ensure CSP 'navigate-to' does not parse.
// Bug 1566149

"use strict";

const TEST_URI =
  "data:text/html;charset=utf8,Web Console navigate-to parse error test";
const TEST_VIOLATION =
  "https://example.com/browser/devtools/client/webconsole/" +
  "test/browser/test-navigate-to-parse-error.html";

const CSP_VIOLATION_MSG =
  "Content Security Policy: Couldn\u2019t process unknown directive \u2018navigate-to\u2019";

add_task(async function() {
  const hud = await openNewTabAndConsole(TEST_URI);
  hud.ui.clearOutput();

  const onRepeatedMessage = waitForRepeatedMessage(hud, CSP_VIOLATION_MSG, 2);
  await loadDocument(TEST_VIOLATION);
  await onRepeatedMessage;

  ok(true, "Received expected messages");
});
