/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test that the invoke getter authorizations are cleared when expected.

const TEST_URI = `data:text/html;charset=utf-8,<!DOCTYPE html>
<head>
  <script>
    /* Create a prototype-less object so popup does not contain native
     * Object prototype properties.
     */
    var obj = props => Object.create(null, Object.getOwnPropertyDescriptors(props));
    let sideEffectVar;
    var foo = obj({
      get bar() {
        sideEffectVar = "from bar";
        return obj({
          get baz() {
            sideEffectVar = "from baz";
            return obj({
              hello: 1,
              world: "",
            });
          },
          bloop: true,
        })
      }
    });
  </script>
</head>
<body>Autocomplete popup - invoke getter cache test</body>`;

add_task(async function () {
  const hud = await openNewTabAndConsole(TEST_URI);
  const { jsterm } = hud;
  const { autocompletePopup } = jsterm;
  const toolbox = gDevTools.getToolboxForTab(gBrowser.selectedTab);

  let tooltip = await setInputValueForGetterConfirmDialog(
    toolbox,
    hud,
    "foo.bar."
  );
  let labelEl = tooltip.querySelector(".confirm-label");
  is(
    labelEl.textContent,
    "Invoke getter foo.bar to retrieve the property list?",
    "Dialog has expected text content"
  );

  info(
    "Check that hitting Tab does invoke the getter and return its properties"
  );
  let onAutocompleteUpdated = jsterm.once("autocomplete-updated");
  EventUtils.synthesizeKey("KEY_Tab");
  await onAutocompleteUpdated;
  ok(autocompletePopup.isOpen, "popup is open after Tab");
  ok(
    hasExactPopupLabels(autocompletePopup, ["baz", "bloop"]),
    "popup has expected items"
  );
  checkInputValueAndCursorPosition(hud, "foo.bar.|");
  is(isConfirmDialogOpened(toolbox), false, "confirm tooltip is now closed");

  info("Close autocomplete popup");
  let onPopupClose = autocompletePopup.once("popup-closed");
  EventUtils.synthesizeKey("KEY_Escape");
  await onPopupClose;

  info(
    "Ctrl+Space again to ensure the autocomplete is shown, not the confirm dialog"
  );
  onAutocompleteUpdated = jsterm.once("autocomplete-updated");
  EventUtils.synthesizeKey(" ", { ctrlKey: true });
  await onAutocompleteUpdated;
  ok(autocompletePopup.isOpen, "popup is open after Ctrl + Space");
  ok(
    hasExactPopupLabels(autocompletePopup, ["baz", "bloop"]),
    "popup has expected items"
  );
  checkInputValueAndCursorPosition(hud, "foo.bar.|");
  is(isConfirmDialogOpened(toolbox), false, "confirm tooltip is not open");

  info(
    "Type a space, then backspace and ensure the autocomplete popup is displayed"
  );
  onAutocompleteUpdated = jsterm.once("autocomplete-updated");
  EventUtils.synthesizeKey(" ");
  await onAutocompleteUpdated;
  is(autocompletePopup.isOpen, true, "Autocomplete popup is still opened");
  ok(
    hasExactPopupLabels(autocompletePopup, ["baz", "bloop"]),
    "popup has expected items"
  );

  onAutocompleteUpdated = jsterm.once("autocomplete-updated");
  EventUtils.synthesizeKey("KEY_Backspace");
  await onAutocompleteUpdated;
  is(autocompletePopup.isOpen, true, "Autocomplete popup is still opened");
  ok(
    hasExactPopupLabels(autocompletePopup, ["baz", "bloop"]),
    "popup has expected items"
  );

  info(
    "Reload the page to ensure asking for autocomplete again show the confirm dialog"
  );
  onPopupClose = autocompletePopup.once("popup-closed");
  await reloadBrowser();
  info("tab reloaded, waiting for the popup to close");
  await onPopupClose;

  info("Press Ctrl+Space to open the confirm dialog again");
  EventUtils.synthesizeKey(" ", { ctrlKey: true });
  await waitFor(() => isConfirmDialogOpened(toolbox));
  ok(true, "Confirm Dialog is shown after tab navigation");
  tooltip = getConfirmDialog(toolbox);
  labelEl = tooltip.querySelector(".confirm-label");
  is(
    labelEl.textContent,
    "Invoke getter foo.bar to retrieve the property list?",
    "Dialog has expected text content"
  );

  info("Close tooltip");
  EventUtils.synthesizeKey("KEY_Escape");
  await waitFor(() => !isConfirmDialogOpened(toolbox));
});
