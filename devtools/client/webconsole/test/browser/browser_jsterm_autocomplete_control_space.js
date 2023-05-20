/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test that Ctrl+Space displays the autocompletion popup when it's hidden.

const TEST_URI = `data:text/html;charset=utf-8,<!DOCTYPE html>
<head>
  <script>
    /* Create a prototype-less object so popup does not contain native
     * Object prototype properties.
     */
    var foo = Object.create(null, Object.getOwnPropertyDescriptors({
      item0: "value0",
      item1: "value1",
    }));
  </script>
</head>
<body>bug 585991 - autocomplete popup ctrl+space usage test</body>`;

add_task(async function () {
  const hud = await openNewTabAndConsole(TEST_URI);
  info("web console opened");

  const { autocompletePopup: popup } = hud.jsterm;

  info("wait for completion: foo.");
  await setInputValueForAutocompletion(hud, "foo.");

  const { itemCount } = popup;
  ok(popup.isOpen, "popup is open");
  ok(itemCount > 0, "popup has items");

  info("Check that Ctrl+Space when the popup is opened has no effect");
  EventUtils.synthesizeKey(" ", { ctrlKey: true });
  ok(popup.isOpen, "The popup wasn't closed on Ctrl+Space");
  is(popup.itemCount, itemCount, "The popup wasn't modified on Ctrl+Space");

  info("press Escape to close the popup");
  let onPopupClose = popup.once("popup-closed");
  EventUtils.synthesizeKey("KEY_Escape");
  await onPopupClose;
  ok(!popup.isOpen, "popup is not open after VK_ESCAPE");

  info("Check that Ctrl+Space opens the popup when it was closed");
  const onAutocompleteUpdated = hud.jsterm.once("autocomplete-updated");
  EventUtils.synthesizeKey(" ", { ctrlKey: true });
  await onAutocompleteUpdated;

  ok(popup.isOpen, "popup opens on Ctrl+Space");
  is(popup.itemCount, itemCount, "popup has the expected items");

  info("Close the popup again");
  onPopupClose = popup.once("popup-closed");
  EventUtils.synthesizeKey("KEY_Escape");
  await onPopupClose;
});
