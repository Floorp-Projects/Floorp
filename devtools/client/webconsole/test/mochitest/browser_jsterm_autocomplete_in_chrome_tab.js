/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

// Tests that code completion works properly in chrome tabs, like about:config.

"use strict";

add_task(async function() {
  // Run test with legacy JsTerm
  await pushPref("devtools.webconsole.jsterm.codeMirror", false);
  await performTests();
  // And then run it with the CodeMirror-powered one.
  await pushPref("devtools.webconsole.jsterm.codeMirror", true);
  await performTests();
});

async function performTests() {
  const hud = await openNewTabAndConsole("about:config");
  ok(hud, "we have a console");
  ok(hud.iframeWindow, "we have the console UI window");

  const { jsterm } = hud;
  ok(jsterm, "we have a jsterm");
  ok(hud.outputNode, "we have an output node");

  // Test typing 'docu'.
  await setInputValueForAutocompletion(hud, "docu");
  checkInputCompletionValue(hud, "    ment", "'docu' completion");
}
