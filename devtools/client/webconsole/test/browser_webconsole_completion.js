/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

// Tests that code completion works properly.

"use strict";

const TEST_URI = "data:text/html;charset=utf8,<p>test code completion";

var jsterm;

add_task(function* () {
  yield loadTab(TEST_URI);

  let hud = yield openConsole();

  jsterm = hud.jsterm;
  let input = jsterm.inputNode;

  // Test typing 'docu'.
  input.value = "docu";
  input.setSelectionRange(4, 4);
  yield complete(jsterm.COMPLETE_HINT_ONLY);

  is(input.value, "docu", "'docu' completion (input.value)");
  is(jsterm.completeNode.value, "    ment", "'docu' completion (completeNode)");

  // Test typing 'docu' and press tab.
  input.value = "docu";
  input.setSelectionRange(4, 4);
  yield complete(jsterm.COMPLETE_FORWARD);

  is(input.value, "document", "'docu' tab completion");
  is(input.selectionStart, 8, "start selection is alright");
  is(input.selectionEnd, 8, "end selection is alright");
  is(jsterm.completeNode.value.replace(/ /g, ""), "", "'docu' completed");

  // Test typing 'window.Ob' and press tab.  Just 'window.O' is
  // ambiguous: could be window.Object, window.Option, etc.
  input.value = "window.Ob";
  input.setSelectionRange(9, 9);
  yield complete(jsterm.COMPLETE_FORWARD);

  is(input.value, "window.Object", "'window.Ob' tab completion");

  // Test typing 'document.getElem'.
  input.value = "document.getElem";
  input.setSelectionRange(16, 16);
  yield complete(jsterm.COMPLETE_FORWARD);

  is(input.value, "document.getElem", "'document.getElem' completion");
  is(jsterm.completeNode.value, "                entsByTagNameNS",
     "'document.getElem' completion");

  // Test pressing tab another time.
  yield jsterm.complete(jsterm.COMPLETE_FORWARD);

  is(input.value, "document.getElem", "'document.getElem' completion");
  is(jsterm.completeNode.value, "                entsByTagName",
     "'document.getElem' another tab completion");

  // Test pressing shift_tab.
  complete(jsterm.COMPLETE_BACKWARD);

  is(input.value, "document.getElem", "'document.getElem' untab completion");
  is(jsterm.completeNode.value, "                entsByTagNameNS",
     "'document.getElem' completion");

  jsterm.clearOutput();

  input.value = "docu";
  yield complete(jsterm.COMPLETE_HINT_ONLY);

  is(jsterm.completeNode.value, "    ment", "'docu' completion");
  yield jsterm.execute();
  is(jsterm.completeNode.value, "", "clear completion on execute()");

  // Test multi-line completion works
  input.value = "console.log('one');\nconsol";
  yield complete(jsterm.COMPLETE_HINT_ONLY);

  is(jsterm.completeNode.value, "                   \n      e",
     "multi-line completion");

  // Test non-object autocompletion.
  input.value = "Object.name.sl";
  yield complete(jsterm.COMPLETE_HINT_ONLY);

  is(jsterm.completeNode.value, "              ice", "non-object completion");

  // Test string literal autocompletion.
  input.value = "'Asimov'.sl";
  yield complete(jsterm.COMPLETE_HINT_ONLY);

  is(jsterm.completeNode.value, "           ice", "string literal completion");

  jsterm = null;
});

function complete(type) {
  let updated = jsterm.once("autocomplete-updated");
  jsterm.complete(type);
  return updated;
}
