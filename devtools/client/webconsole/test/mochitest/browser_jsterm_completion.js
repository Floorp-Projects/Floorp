/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

// Tests that code completion works properly.

"use strict";

const TEST_URI = "data:text/html;charset=utf8,<p>test code completion";

add_task(async function() {
  // Run test with legacy JsTerm
  await performTests();
  // And then run it with the CodeMirror-powered one.
  await pushPref("devtools.webconsole.jsterm.codeMirror", true);
  await performTests();
});

async function performTests() {
  const {jsterm, ui} = await openNewTabAndConsole(TEST_URI);
  const {autocompletePopup} = jsterm;

  // Test typing 'docu'.
  await jstermSetValueAndComplete(jsterm, "docu");
  is(jsterm.getInputValue(), "docu", "'docu' completion (input.value)");
  checkJsTermCompletionValue(jsterm, "    ment", "'docu' completion (completeNode)");
  is(autocompletePopup.items.length, 1, "autocomplete popup has 1 item");
  ok(autocompletePopup.isOpen, "autocomplete popup is open with 1 item");

  // Test typing 'docu' and press tab.
  await jstermSetValueAndComplete(jsterm, "docu", undefined, jsterm.COMPLETE_FORWARD);
  is(jsterm.getInputValue(), "document", "'docu' tab completion");

  checkJsTermCursor(jsterm, "document".length, "cursor is at the end of 'document'");
  is(getJsTermCompletionValue(jsterm).replace(/ /g, ""), "", "'docu' completed");

  // Test typing 'window.Ob' and press tab.  Just 'window.O' is
  // ambiguous: could be window.Object, window.Option, etc.
  await jstermSetValueAndComplete(jsterm, "window.Ob", undefined,
                                  jsterm.COMPLETE_FORWARD);
  is(jsterm.getInputValue(), "window.Object", "'window.Ob' tab completion");

  // Test typing 'document.getElem'.
  await jstermSetValueAndComplete(
    jsterm, "document.getElem", undefined, jsterm.COMPLETE_FORWARD);
  is(jsterm.getInputValue(), "document.getElem", "'document.getElem' completion");
  checkJsTermCompletionValue(jsterm, "                entsByTagNameNS",
     "'document.getElem' completion");

  // Test pressing tab another time.
  await jsterm.complete(jsterm.COMPLETE_FORWARD);
  is(jsterm.getInputValue(), "document.getElem", "'document.getElem' completion");
  checkJsTermCompletionValue(jsterm, "                entsByTagName",
     "'document.getElem' another tab completion");

  // Test pressing shift_tab.
  await jstermComplete(jsterm, jsterm.COMPLETE_BACKWARD);
  is(jsterm.getInputValue(), "document.getElem", "'document.getElem' untab completion");
  checkJsTermCompletionValue(jsterm, "                entsByTagNameNS",
     "'document.getElem' completion");

  ui.clearOutput();

  await jstermSetValueAndComplete(jsterm, "docu");
  checkJsTermCompletionValue(jsterm, "    ment", "'docu' completion");

  await jsterm.execute();
  checkJsTermCompletionValue(jsterm, "", "clear completion on execute()");

  // Test multi-line completion works
  await jstermSetValueAndComplete(jsterm, "console.log('one');\nconsol");
  checkJsTermCompletionValue(jsterm, "                   \n      e",
     "multi-line completion");

  // Test non-object autocompletion.
  await jstermSetValueAndComplete(jsterm, "Object.name.sl");
  checkJsTermCompletionValue(jsterm, "              ice", "non-object completion");

  // Test string literal autocompletion.
  await jstermSetValueAndComplete(jsterm, "'Asimov'.sl");
  checkJsTermCompletionValue(jsterm, "           ice", "string literal completion");
}
