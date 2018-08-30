/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

// Tests that code completion works properly.

"use strict";

const TEST_URI = `data:text/html;charset=utf8,<p>test code completion
  <script>
    foobar = true;
  </script>`;

add_task(async function() {
  // Run test with legacy JsTerm
  await pushPref("devtools.webconsole.jsterm.codeMirror", false);
  await performTests();
  // And then run it with the CodeMirror-powered one.
  await pushPref("devtools.webconsole.jsterm.codeMirror", true);
  await performTests();
});

async function performTests() {
  const {jsterm, ui} = await openNewTabAndConsole(TEST_URI);
  const {autocompletePopup} = jsterm;

  // Test typing 'docu'.
  await setInputValueForAutocompletion(jsterm, "foob");
  is(jsterm.getInputValue(), "foob", "'foob' completion (input.value)");
  checkJsTermCompletionValue(jsterm, "    ar", "'foob' completion (completeNode)");
  is(autocompletePopup.items.length, 1, "autocomplete popup has 1 item");
  is(autocompletePopup.isOpen, false, "autocomplete popup is not open");

  // Test typing 'docu' and press tab.
  EventUtils.synthesizeKey("KEY_Tab");
  is(jsterm.getInputValue(), "foobar", "'foob' tab completion");

  checkJsTermCursor(jsterm, "foobar".length, "cursor is at the end of 'foobar'");
  is(getJsTermCompletionValue(jsterm).replace(/ /g, ""), "", "'foob' completed");

  // Test typing 'window.Ob' and press tab.  Just 'window.O' is
  // ambiguous: could be window.Object, window.Option, etc.
  await setInputValueForAutocompletion(jsterm, "window.Ob");
  EventUtils.synthesizeKey("KEY_Tab");
  is(jsterm.getInputValue(), "window.Object", "'window.Ob' tab completion");

  // Test typing 'document.getElem'.
  const onPopupOpened = autocompletePopup.once("popup-opened");
  await setInputValueForAutocompletion(jsterm, "document.getElem");
  is(jsterm.getInputValue(), "document.getElem", "'document.getElem' completion");
  checkJsTermCompletionValue(jsterm, "                entById",
     "'document.getElem' completion");

  // Test pressing key down.
  await onPopupOpened;
  EventUtils.synthesizeKey("KEY_ArrowDown");
  is(jsterm.getInputValue(), "document.getElem", "'document.getElem' completion");
  checkJsTermCompletionValue(jsterm, "                entsByClassName",
     "'document.getElem' another tab completion");

  // Test pressing key up.
  EventUtils.synthesizeKey("KEY_ArrowUp");
  await waitFor(() => (getJsTermCompletionValue(jsterm) || "").includes("entById"));
  is(jsterm.getInputValue(), "document.getElem", "'document.getElem' untab completion");
  checkJsTermCompletionValue(jsterm, "                entById",
     "'document.getElem' completion");

  ui.clearOutput();

  await setInputValueForAutocompletion(jsterm, "docu");
  checkJsTermCompletionValue(jsterm, "    ment", "'docu' completion");

  let onAutocompletUpdated = jsterm.once("autocomplete-updated");
  await jsterm.execute();
  checkJsTermCompletionValue(jsterm, "", "clear completion on execute()");

  // Test multi-line completion works. We can't use setInputValueForAutocompletion because
  // it would trigger an evaluation (because of the new line, an Enter keypress is
  // simulated).
  onAutocompletUpdated = jsterm.once("autocomplete-updated");
  jsterm.setInputValue("console.log('one');\n");
  EventUtils.sendString("consol");
  await onAutocompletUpdated;
  checkJsTermCompletionValue(jsterm, "\n      e", "multi-line completion");

  // Test multi-line completion works even if there is text after the cursor
  onAutocompletUpdated = jsterm.once("autocomplete-updated");
  jsterm.setInputValue("{\n\n}");
  EventUtils.synthesizeKey("KEY_ArrowUp");
  EventUtils.sendString("console.g");
  await onAutocompletUpdated;
  checkJsTermValueAndCursor(jsterm, "{\nconsole.g|\n}");
  checkJsTermCompletionValue(jsterm, "\n         roup", "multi-line completion");
  is(autocompletePopup.isOpen, true, "popup is opened");

  // Test non-object autocompletion.
  await setInputValueForAutocompletion(jsterm, "Object.name.sl");
  checkJsTermCompletionValue(jsterm, "              ice", "non-object completion");

  // Test string literal autocompletion.
  await setInputValueForAutocompletion(jsterm, "'Asimov'.sl");
  checkJsTermCompletionValue(jsterm, "           ice", "string literal completion");
}
