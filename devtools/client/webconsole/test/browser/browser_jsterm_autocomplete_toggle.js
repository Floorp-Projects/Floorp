/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

// Test for the input autocomplete option: check if the preference toggles the
// autocomplete feature in the console output. See bug 1593607.

"use strict";

const TEST_URI = `data:text/html;charset=utf-8,<!DOCTYPE html>`;
const PREF_INPUT_AUTOCOMPLETE = "devtools.webconsole.input.autocomplete";

add_task(async function () {
  // making sure that input autocomplete is true at the start of test
  await pushPref(PREF_INPUT_AUTOCOMPLETE, true);
  const hud = await openNewTabAndConsole(TEST_URI);

  info(
    "Check that console settings contain autocomplete input and its checked"
  );
  await checkConsoleSettingState(
    hud,
    ".webconsole-console-settings-menu-item-autocomplete",
    true
  );

  info("Check that popup opens");
  const { jsterm } = hud;

  const { autocompletePopup: popup } = jsterm;

  info(`Enter "w"`);
  await setInputValueForAutocompletion(hud, "w");

  ok(popup.isOpen, "autocomplete popup opens up");

  info("Clear input value");
  let onPopupClosed = popup.once("popup-closed");
  setInputValue(hud, "");
  await onPopupClosed;
  ok(!popup.open, "autocomplete popup closed");

  info("toggle autocomplete preference");

  await toggleConsoleSetting(
    hud,
    ".webconsole-console-settings-menu-item-autocomplete"
  );

  info("Checking that popup do not show");
  info(`Enter "w"`);
  setInputValue(hud, "w");
  // delay of 2 seconds.
  await wait(2000);
  ok(!popup.isOpen, "popup is not open");

  info("toggling autocomplete pref back to true");
  await toggleConsoleSetting(
    hud,
    ".webconsole-console-settings-menu-item-autocomplete"
  );

  const prefValue = Services.prefs.getBoolPref(PREF_INPUT_AUTOCOMPLETE);
  ok(prefValue, "autocomplete pref value set to true");

  info("Check that popup opens");

  info(`Enter "w"`);
  await setInputValueForAutocompletion(hud, "w");

  ok(popup.isOpen, "autocomplete popup opens up");

  info("Clear input value");
  onPopupClosed = popup.once("popup-closed");
  setInputValue(hud, "");
  await onPopupClosed;
  ok(!popup.open, "autocomplete popup closed");
});
