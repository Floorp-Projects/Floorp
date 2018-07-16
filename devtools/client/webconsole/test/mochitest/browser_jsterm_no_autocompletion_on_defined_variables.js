/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

// Tests for bug 704295

"use strict";

const TEST_URI = "http://example.com/browser/devtools/client/webconsole/" +
                 "test/mochitest/test-console.html";

add_task(async function() {
  // Run test with legacy JsTerm
  await performTests();
  // And then run it with the CodeMirror-powered one.
  await pushPref("devtools.webconsole.jsterm.codeMirror", true);
  await performTests();
});

async function performTests() {
  const hud = await openNewTabAndConsole(TEST_URI);
  const jsterm = hud.jsterm;

  // Test typing 'var d = 5;' and press RETURN
  jsterm.setInputValue("var d = ");
  EventUtils.sendString("5;");
  is(jsterm.getInputValue(), "var d = 5;", "var d = 5;");
  checkJsTermCompletionValue(jsterm, "", "no completion");
  EventUtils.synthesizeKey("KEY_Enter");
  checkJsTermCompletionValue(jsterm, "", "clear completion on execute()");

  // Test typing 'var a = d' and press RETURN
  jsterm.setInputValue("var a = ");
  EventUtils.sendString("d");
  is(jsterm.getInputValue(), "var a = d", "var a = d");
  checkJsTermCompletionValue(jsterm, "", "no completion");
  EventUtils.synthesizeKey("KEY_Enter");
  checkJsTermCompletionValue(jsterm, "", "clear completion on execute()");
}
