/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

// Tests that, when the user types an extraneous closing bracket, no error
// appears. See Bug 592442.

"use strict";

const TEST_URI = "data:text/html;charset=utf-8,test for bug 592442";

add_task(async function() {
  // Run test with legacy JsTerm
  await performTests();
  // And then run it with the CodeMirror-powered one.
  await pushPref("devtools.webconsole.jsterm.codeMirror", true);
  await performTests();
});

async function performTests() {
  const { jsterm } = await openNewTabAndConsole(TEST_URI);

  try {
    await jstermSetValueAndComplete(jsterm, "document.getElementById)");
    ok(true, "no error was thrown when an extraneous bracket was inserted");
  } catch (ex) {
    ok(false, "an error was thrown when an extraneous bracket was inserted");
  }
}
