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
  const hud = await openNewTabAndConsole(TEST_URI);
  const { jsterm, ui } = hud;
  const { autocompletePopup } = jsterm;

  // Test typing 'docu'.
  await setInputValueForAutocompletion(hud, "foob");
  is(getInputValue(hud), "foob", "'foob' completion (input.value)");
  checkInputCompletionValue(hud, "ar", "'foob' completion (completeNode)");
  is(autocompletePopup.items.length, 1, "autocomplete popup has 1 item");
  is(autocompletePopup.isOpen, false, "autocomplete popup is not open");

  // Test typing 'docu' and press tab.
  EventUtils.synthesizeKey("KEY_Tab");
  is(getInputValue(hud), "foobar", "'foob' tab completion");

  checkInputCursorPosition(
    hud,
    "foobar".length,
    "cursor is at the end of 'foobar'"
  );
  is(getInputCompletionValue(hud).replace(/ /g, ""), "", "'foob' completed");

  // Test typing 'window.Ob' and press tab.  Just 'window.O' is
  // ambiguous: could be window.Object, window.Option, etc.
  await setInputValueForAutocompletion(hud, "window.Ob");
  EventUtils.synthesizeKey("KEY_Tab");
  is(getInputValue(hud), "window.Object", "'window.Ob' tab completion");

  // Test typing 'document.getElem'.
  const onPopupOpened = autocompletePopup.once("popup-opened");
  await setInputValueForAutocompletion(hud, "document.getElem");
  is(getInputValue(hud), "document.getElem", "'document.getElem' completion");
  checkInputCompletionValue(hud, "entById", "'document.getElem' completion");

  // Test pressing key down.
  await onPopupOpened;
  EventUtils.synthesizeKey("KEY_ArrowDown");
  is(getInputValue(hud), "document.getElem", "'document.getElem' completion");
  checkInputCompletionValue(
    hud,
    "entsByClassName",
    "'document.getElem' another tab completion"
  );

  // Test pressing key up.
  EventUtils.synthesizeKey("KEY_ArrowUp");
  await waitFor(() => (getInputCompletionValue(hud) || "").includes("entById"));
  is(
    getInputValue(hud),
    "document.getElem",
    "'document.getElem' untab completion"
  );
  checkInputCompletionValue(hud, "entById", "'document.getElem' completion");

  ui.clearOutput();

  await setInputValueForAutocompletion(hud, "docu");
  checkInputCompletionValue(hud, "ment", "'docu' completion");

  let onAutocompletUpdated = jsterm.once("autocomplete-updated");
  EventUtils.synthesizeKey("KEY_Enter");
  await onAutocompletUpdated;
  checkInputCompletionValue(hud, "", "clear completion on execute()");

  // Test multi-line completion works. We can't use setInputValueForAutocompletion because
  // it would trigger an evaluation (because of the new line, an Enter keypress is
  // simulated).
  onAutocompletUpdated = jsterm.once("autocomplete-updated");
  setInputValue(hud, "console.log('one');\n");
  EventUtils.sendString("consol");
  await onAutocompletUpdated;
  checkInputCompletionValue(hud, "e", "multi-line completion");

  // Test multi-line completion works even if there is text after the cursor
  onAutocompletUpdated = jsterm.once("autocomplete-updated");
  setInputValue(hud, "{\n\n}");
  EventUtils.synthesizeKey("KEY_ArrowUp");
  EventUtils.sendString("console.g");
  await onAutocompletUpdated;
  checkInputValueAndCursorPosition(hud, "{\nconsole.g|\n}");
  checkInputCompletionValue(hud, "roup", "multi-line completion");
  is(autocompletePopup.isOpen, true, "popup is opened");

  // Test non-object autocompletion.
  await setInputValueForAutocompletion(hud, "Object.name.sl");
  checkInputCompletionValue(hud, "ice", "non-object completion");

  // Test string literal autocompletion.
  await setInputValueForAutocompletion(hud, "'Asimov'.sl");
  checkInputCompletionValue(hud, "ice", "string literal completion");
});
