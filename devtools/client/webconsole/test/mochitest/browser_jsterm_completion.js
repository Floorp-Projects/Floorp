/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

// Tests that code completion works properly.

"use strict";

const TEST_URI = "data:text/html;charset=utf8,<p>test code completion";

add_task(async function() {
  const {jsterm} = await openNewTabAndConsole(TEST_URI);
  const input = jsterm.inputNode;

  // Test typing 'docu'.
  await jstermSetValueAndComplete(jsterm, "docu");
  is(input.value, "docu", "'docu' completion (input.value)");
  is(jsterm.completeNode.value, "    ment", "'docu' completion (completeNode)");

  // Test typing 'docu' and press tab.
  await jstermSetValueAndComplete(jsterm, "docu", undefined, jsterm.COMPLETE_FORWARD);
  is(input.value, "document", "'docu' tab completion");
  is(input.selectionStart, 8, "start selection is alright");
  is(input.selectionEnd, 8, "end selection is alright");
  is(jsterm.completeNode.value.replace(/ /g, ""), "", "'docu' completed");

  // Test typing 'window.Ob' and press tab.  Just 'window.O' is
  // ambiguous: could be window.Object, window.Option, etc.
  await jstermSetValueAndComplete(jsterm, "window.Ob", undefined,
                                  jsterm.COMPLETE_FORWARD);
  is(input.value, "window.Object", "'window.Ob' tab completion");

  // Test typing 'document.getElem'.
  await jstermSetValueAndComplete(
    jsterm, "document.getElem", undefined, jsterm.COMPLETE_FORWARD);
  is(input.value, "document.getElem", "'document.getElem' completion");
  is(jsterm.completeNode.value, "                entsByTagNameNS",
     "'document.getElem' completion");

  // Test pressing tab another time.
  await jsterm.complete(jsterm.COMPLETE_FORWARD);
  is(input.value, "document.getElem", "'document.getElem' completion");
  is(jsterm.completeNode.value, "                entsByTagName",
     "'document.getElem' another tab completion");

  // Test pressing shift_tab.
  await jstermComplete(jsterm, jsterm.COMPLETE_BACKWARD);
  is(input.value, "document.getElem", "'document.getElem' untab completion");
  is(jsterm.completeNode.value, "                entsByTagNameNS",
     "'document.getElem' completion");

  jsterm.clearOutput();

  await jstermSetValueAndComplete(jsterm, "docu");
  is(jsterm.completeNode.value, "    ment", "'docu' completion");

  await jsterm.execute();
  is(jsterm.completeNode.value, "", "clear completion on execute()");

  // Test multi-line completion works
  await jstermSetValueAndComplete(jsterm, "console.log('one');\nconsol");
  is(jsterm.completeNode.value, "                   \n      e",
     "multi-line completion");

  // Test non-object autocompletion.
  await jstermSetValueAndComplete(jsterm, "Object.name.sl");
  is(jsterm.completeNode.value, "              ice", "non-object completion");

  // Test string literal autocompletion.
  await jstermSetValueAndComplete(jsterm, "'Asimov'.sl");
  is(jsterm.completeNode.value, "           ice", "string literal completion");
});
