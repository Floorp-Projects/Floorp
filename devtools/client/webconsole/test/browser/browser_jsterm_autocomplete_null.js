/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

add_task(async function() {
  await pushPref("devtools.chrome.enabled", true);
  await addTab("about:blank");

  info(`Open browser console with ctrl-shift-j`);
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
  execute(hud, "globalThis.nullVar = null;");

  info(`Check completion suggestions for "null"`);
  const onPopUpOpen = popup.once("popup-opened");
  EventUtils.sendString("null", hud.iframeWindow);
  await onPopUpOpen;
  ok(popup.isOpen, "popup is open");
  const expectedPopupItems = ["null", "nullVar"];
  is(
    popup.items.map(i => i.label).join("-"),
    expectedPopupItems.join("-"),
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

  info(`Check that no error was logged`);
  await waitFor(() => findMessage(hud, "", ".message.error")).then(
    message => {
      ok(false, `Got error ${JSON.stringify(message.textContent)}`);
    },
    error => {
      if (!error.includes("waitFor - timed out")) {
        throw error;
      }
      ok(true, `No error was logged`);
    }
  );

  info(`Cleanup`);
  execute(hud, "delete globalThis.nullVar;");
});
