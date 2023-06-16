/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

add_task(async function () {
  await pushPref("devtools.chrome.enabled", true);
  await addTab("about:blank");

  info(`Open browser console with ctrl-shift-j`);
  // we're using the browser console so we can check for error messages that would be
  // caused by console code.
  const opened = waitForBrowserConsole();
  EventUtils.synthesizeKey("j", { accelKey: true, shiftKey: true }, window);
  const hud = await opened;
  const { jsterm } = hud;
  const { autocompletePopup: popup } = jsterm;

  info(`Clear existing messages`);
  const onMessagesCleared = hud.ui.once("messages-cleared");
  await clearOutput(hud);
  await onMessagesCleared;

  info(`Create a null variable`);
  // Using the commands directly as we don't want to impact the UI state.
  await hud.commands.scriptCommand.execute("globalThis.nullVar = null");

  info(`Check completion suggestions for "null"`);
  await setInputValueForAutocompletion(hud, "null");
  ok(popup.isOpen, "popup is open");
  const expectedPopupItems = ["null", "nullVar"];
  ok(
    hasExactPopupLabels(popup, expectedPopupItems),
    "popup has expected items"
  );

  info(`Check completion suggestions for "null."`);
  let onAutocompleteUpdated = jsterm.once("autocomplete-updated");
  EventUtils.sendString(".", hud.iframeWindow);
  await onAutocompleteUpdated;
  is(popup.itemCount, 0, "popup has no items");

  info(`Check completion suggestions for "null"`);
  onAutocompleteUpdated = jsterm.once("autocomplete-updated");
  EventUtils.synthesizeKey("KEY_Backspace", undefined, hud.iframeWindow);
  await onAutocompleteUpdated;
  is(popup.itemCount, 2, "popup has 2 items");

  info(`Check completion suggestions for "nullVar"`);
  onAutocompleteUpdated = jsterm.once("autocomplete-updated");
  EventUtils.sendString("Var.", hud.iframeWindow);
  await onAutocompleteUpdated;
  is(popup.itemCount, 0, "popup has no items");
  is(popup.isOpen, false, "popup is closed");

  info(`Check that no error was logged`);
  await waitFor(() => findErrorMessage(hud, "", ":not(.network)")).then(
    message => {
      ok(false, `Got error ${JSON.stringify(message.textContent)}`);
    },
    error => {
      if (!error.message.includes("Failed waitFor")) {
        throw error;
      }
      ok(true, `No error was logged`);
    }
  );

  info(`Cleanup`);
  await hud.commands.scriptCommand.execute("delete globalThis.nullVar");
});
