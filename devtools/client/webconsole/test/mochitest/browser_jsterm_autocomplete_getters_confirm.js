/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test that accessing properties with getters displays the confirm dialog to invoke them,
// and then displays the autocomplete popup with the results.

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
      },
      get rab() {
        return "";
      }
    });
  </script>
</head>
<body>Autocomplete popup - invoke getter usage test</body>`;

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
    "window.foo.bar."
  );
  let labelEl = tooltip.querySelector(".confirm-label");
  is(
    labelEl.textContent,
    "Invoke getter window.foo.bar to retrieve the property list?",
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
  checkInputValueAndCursorPosition(hud, "window.foo.bar.|");
  is(isConfirmDialogOpened(toolbox), false, "confirm tooltip is now closed");

  let onPopUpClose = autocompletePopup.once("popup-closed");
  EventUtils.synthesizeKey("KEY_Enter");
  await onPopUpClose;
  checkInputValueAndCursorPosition(hud, "window.foo.bar.baz|");

  info(
    "Check that the invoke tooltip is displayed when performing an element access"
  );
  EventUtils.sendString("[");
  await waitFor(() => isConfirmDialogOpened(toolbox));

  tooltip = getConfirmDialog(toolbox);
  labelEl = tooltip.querySelector(".confirm-label");
  is(
    labelEl.textContent,
    "Invoke getter window.foo.bar.baz to retrieve the property list?",
    "Dialog has expected text content"
  );

  info(
    "Check that hitting Tab does invoke the getter and return its properties"
  );
  onPopUpOpen = autocompletePopup.once("popup-opened");
  EventUtils.synthesizeKey("KEY_Tab");
  await onPopUpOpen;
  ok(autocompletePopup.isOpen, "popup is open after Tab");
  is(
    getAutocompletePopupLabels(autocompletePopup).join("-"),
    `"hello"-"world"`,
    "popup has expected items"
  );
  checkInputValueAndCursorPosition(hud, "window.foo.bar.baz[|");
  is(isConfirmDialogOpened(toolbox), false, "confirm tooltip is now closed");

  onPopUpClose = autocompletePopup.once("popup-closed");
  EventUtils.synthesizeKey("KEY_Tab");
  await onPopUpClose;
  checkInputValueAndCursorPosition(hud, `window.foo.bar.baz["hello"]|`);

  info("Check that autocompletion work on a getter result");
  onPopUpOpen = autocompletePopup.once("popup-opened");
  EventUtils.sendString(".");
  await onPopUpOpen;
  ok(autocompletePopup.isOpen, "got items of getter result");
  ok(
    getAutocompletePopupLabels(autocompletePopup).includes("toExponential"),
    "popup has expected items"
  );

  tooltip = await setInputValueForGetterConfirmDialog(
    toolbox,
    hud,
    "window.foo.rab."
  );
  labelEl = tooltip.querySelector(".confirm-label");
  is(
    labelEl.textContent,
    "Invoke getter window.foo.rab to retrieve the property list?",
    "Dialog has expected text content"
  );

  info(
    "Check clicking the confirm button invokes the getter and return its properties"
  );
  onPopUpOpen = autocompletePopup.once("popup-opened");
  EventUtils.synthesizeMouseAtCenter(
    tooltip.querySelector(".confirm-button"),
    {
      type: "mousedown",
    },
    toolbox.win
  );
  await onPopUpOpen;
  ok(
    autocompletePopup.isOpen,
    "popup is open after clicking on the confirm button"
  );
  ok(
    getAutocompletePopupLabels(autocompletePopup).includes("startsWith"),
    "popup has expected items"
  );
  checkInputValueAndCursorPosition(hud, "window.foo.rab.|");
  is(isConfirmDialogOpened(toolbox), false, "confirm tooltip is now closed");

  info("Open the tooltip again");
  tooltip = await setInputValueForGetterConfirmDialog(
    toolbox,
    hud,
    "window.foo.bar."
  );
  labelEl = tooltip.querySelector(".confirm-label");
  is(
    labelEl.textContent,
    "Invoke getter window.foo.bar to retrieve the property list?",
    "Dialog has expected text content"
  );

  info("Check that Space invokes the getter and return its properties");
  onPopUpOpen = autocompletePopup.once("popup-opened");
  EventUtils.synthesizeKey(" ");
  await onPopUpOpen;
  ok(autocompletePopup.isOpen, "popup is open after space");
  is(
    getAutocompletePopupLabels(autocompletePopup).join("-"),
    "baz-bloop",
    "popup has expected items"
  );
  checkInputValueAndCursorPosition(hud, "window.foo.bar.|");
  is(isConfirmDialogOpened(toolbox), false, "confirm tooltip is now closed");
}
