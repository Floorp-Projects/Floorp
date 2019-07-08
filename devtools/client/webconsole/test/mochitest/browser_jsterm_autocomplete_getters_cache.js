/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test that the invoke getter authorizations are cleared when expected.

const TEST_URI = `data:text/html;charset=utf-8,
<head>
  <script>
    /* Create a prototype-less object so popup does not contain native
     * Object prototype properties.
     */
    var obj = props => Object.create(null, Object.getOwnPropertyDescriptors(props));
    window.foo = obj({
      get bar() {
        return obj({
          get baz() {
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

add_task(async function() {
  // Run test with legacy JsTerm
  await pushPref("devtools.webconsole.jsterm.codeMirror", false);
  await performTests();
  // And then run it with the CodeMirror-powered one.
  await pushPref("devtools.webconsole.jsterm.codeMirror", true);
  await performTests();
});

async function performTests() {
  const hud = await openNewTabAndConsole(TEST_URI);
  const { jsterm } = hud;
  const { autocompletePopup } = jsterm;
  const target = await TargetFactory.forTab(gBrowser.selectedTab);
  const toolbox = gDevTools.getToolbox(target);

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
    "Check that hitting Enter does invoke the getter and return its properties"
  );
  let onPopUpOpen = autocompletePopup.once("popup-opened");
  EventUtils.synthesizeKey("KEY_Enter");
  await onPopUpOpen;
  ok(autocompletePopup.isOpen, "popup is open after Enter");
  is(
    getAutocompletePopupLabels(autocompletePopup).join("-"),
    "baz-bloop",
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
  onPopUpOpen = autocompletePopup.once("popup-opened");
  EventUtils.synthesizeKey(" ", { ctrlKey: true });
  await onPopUpOpen;
  is(
    getAutocompletePopupLabels(autocompletePopup).join("-"),
    "baz-bloop",
    "popup has expected items"
  );
  checkInputValueAndCursorPosition(hud, "foo.bar.|");
  is(isConfirmDialogOpened(toolbox), false, "confirm tooltip is not open");

  info(
    "Type a space, then backspace and ensure the autocomplete popup is displayed"
  );
  let onAutocompleteUpdate = jsterm.once("autocomplete-updated");
  EventUtils.synthesizeKey(" ");
  await onAutocompleteUpdate;
  is(autocompletePopup.isOpen, true, "Autocomplete popup is still opened");
  is(
    getAutocompletePopupLabels(autocompletePopup).join("-"),
    "baz-bloop",
    "popup has expected items"
  );

  onAutocompleteUpdate = jsterm.once("autocomplete-updated");
  EventUtils.synthesizeKey("KEY_Backspace");
  await onAutocompleteUpdate;
  is(autocompletePopup.isOpen, true, "Autocomplete popup is still opened");
  is(
    getAutocompletePopupLabels(autocompletePopup).join("-"),
    "baz-bloop",
    "popup has expected items"
  );

  info(
    "Reload the page to ensure asking for autocomplete again show the confirm dialog"
  );
  onPopupClose = autocompletePopup.once("popup-closed");
  await refreshTab();
  await onPopupClose;

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
}
