/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test that properties starting with underscores or dollars can be
// autocompleted (bug 967468).
const TEST_URI = "data:text/html;charset=utf8,test autocompletion with $ or _";

add_task(async function() {
  // Run test with legacy JsTerm
  await performTests();
  // And then run it with the CodeMirror-powered one.
  await pushPref("devtools.webconsole.jsterm.codeMirror", true);
  await performTests();
});

async function performTests() {
  const { jsterm } = await openNewTabAndConsole(TEST_URI);

  await jsterm.execute("var testObject = {$$aaab: '', $$aaac: ''}");

  // Should work with bug 967468.
  await testAutocomplete(jsterm, "Object.__d");
  await testAutocomplete(jsterm, "testObject.$$a");

  // Here's when things go wrong in bug 967468.
  await testAutocomplete(jsterm, "Object.__de");
  await testAutocomplete(jsterm, "testObject.$$aa");

  // Should work with bug 1207868.
  await jsterm.execute("let foobar = {a: ''}; const blargh = {a: 1};");
  await testAutocomplete(jsterm, "foobar");
  await testAutocomplete(jsterm, "blargh");
  await testAutocomplete(jsterm, "foobar.a");
  await testAutocomplete(jsterm, "blargh.a");
}

async function testAutocomplete(jsterm, inputString) {
  jsterm.setInputValue(inputString);
  await new Promise(resolve => jsterm.complete(jsterm.COMPLETE_HINT_ONLY, resolve));

  const popup = jsterm.autocompletePopup;
  ok(popup.itemCount > 0, `There's ${popup.itemCount} suggestions for '${inputString}'`);
}
