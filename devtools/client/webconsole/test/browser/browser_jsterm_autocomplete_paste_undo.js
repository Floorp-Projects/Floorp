/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const TEST_URI = "data:text/html;charset=utf-8,<p>test for bug 642615</p>";

XPCOMUtils.defineLazyServiceGetter(
  this,
  "clipboardHelper",
  "@mozilla.org/widget/clipboardhelper;1",
  "nsIClipboardHelper"
);
const stringToCopy = "foobazbarBug642615";

add_task(async function() {
  const hud = await openNewTabAndConsole(TEST_URI);
  const { jsterm, ui } = hud;
  ui.clearOutput();
  ok(!getInputCompletionValue(hud), "no completeNode.value");

  setInputValue(hud, "doc");

  info("wait for completion value after typing 'docu'");
  let onAutocompleteUpdated = jsterm.once("autocomplete-updated");
  EventUtils.sendString("u");
  await onAutocompleteUpdated;

  const completionValue = getInputCompletionValue(hud);

  info(`Copy "${stringToCopy}" in clipboard`);
  await waitForClipboardPromise(
    () => clipboardHelper.copyString(stringToCopy),
    stringToCopy
  );

  setInputValue(hud, "docu");
  info("wait for completion update after clipboard paste");
  onAutocompleteUpdated = jsterm.once("autocomplete-updated");
  EventUtils.synthesizeKey("v", { accelKey: true });

  await onAutocompleteUpdated;

  ok(!getInputCompletionValue(hud), "no completion value after paste");

  info("wait for completion update after undo");
  onAutocompleteUpdated = jsterm.once("autocomplete-updated");

  EventUtils.synthesizeKey("z", { accelKey: true });

  await onAutocompleteUpdated;

  checkInputCompletionValue(
    hud,
    completionValue,
    "same completeNode.value after undo"
  );

  info("wait for completion update after clipboard paste (ctrl-v)");
  onAutocompleteUpdated = jsterm.once("autocomplete-updated");

  EventUtils.synthesizeKey("v", { accelKey: true });

  await onAutocompleteUpdated;
  ok(!getInputCompletionValue(hud), "no completion value after paste (ctrl-v)");
});
