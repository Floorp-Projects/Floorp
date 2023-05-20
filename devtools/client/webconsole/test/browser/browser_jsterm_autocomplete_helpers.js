/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

// Tests that the autocompletion results contain the names of JSTerm helpers.
// See Bug 686937.

"use strict";

const TEST_URI =
  "data:text/html;charset=utf8,<!DOCTYPE html><p>test JSTerm Helpers autocomplete";

add_task(async function () {
  await pushPref("devtools.editor.autoclosebrackets", false);
  const hud = await openNewTabAndConsole(TEST_URI);
  await testInspectAutoCompletion(hud, "i", true);
  await testInspectAutoCompletion(hud, "window.", false);
  await testInspectAutoCompletion(hud, "dump(i", true);
  await testInspectAutoCompletion(hud, "window.dump(i", true);

  info("Close autocomplete popup");
  await closeAutocompletePopup(hud);
});

async function testInspectAutoCompletion(hud, inputValue, expectInspect) {
  await setInputValueForAutocompletion(hud, inputValue);
  is(
    hasPopupLabel(hud.jsterm.autocompletePopup, "inspect"),
    expectInspect,
    `autocomplete results${
      expectInspect ? "" : " does not"
    } contain helper 'inspect'`
  );
}
