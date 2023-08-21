/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test that the confirm dialog can be closed with different actions.

const TEST_URI = `data:text/html;charset=utf-8,<!DOCTYPE html>
<head>
  <script>
    let sideEffect;
    window.foo = {
      get rab() {
        sideEffect = "getRab";
        return "rab";
      }
    };
  </script>
</head>
<body>Autocomplete popup - invoke getter - close dialog test</body>`;

add_task(async function () {
  const hud = await openNewTabAndConsole(TEST_URI);
  const { jsterm } = hud;
  const toolbox = gDevTools.getToolboxForTab(gBrowser.selectedTab);

  let tooltip = await setInputValueForGetterConfirmDialog(
    toolbox,
    hud,
    "foo.rab."
  );
  let labelEl = tooltip.querySelector(".confirm-label");
  is(
    labelEl.textContent,
    "Invoke getter foo.rab to retrieve the property list?",
    "Dialog has expected text content"
  );

  info("Check that Escape closes the confirm tooltip");
  EventUtils.synthesizeKey("KEY_Escape");
  await waitFor(() => !isConfirmDialogOpened(toolbox));

  info("Check that typing a letter won't show the tooltip");
  const onAutocompleteUpdate = jsterm.once("autocomplete-updated");
  EventUtils.sendString("t");
  await onAutocompleteUpdate;
  is(isConfirmDialogOpened(toolbox), false, "The confirm dialog is not open");

  info("Check that Ctrl+space show the confirm tooltip again");
  EventUtils.synthesizeKey(" ", { ctrlKey: true });
  await waitFor(() => isConfirmDialogOpened(toolbox));
  tooltip = getConfirmDialog(toolbox);
  labelEl = tooltip.querySelector(".confirm-label");
  is(
    labelEl.textContent,
    "Invoke getter foo.rab to retrieve the property list?",
    "Dialog has expected text content"
  );

  info("Check that clicking on the close button closes the tooltip");
  const closeButtonEl = tooltip.querySelector(".close-confirm-dialog-button");
  is(closeButtonEl.title, "Close (Esc)", "Close button has the expected title");
  closeButtonEl.click();
  await waitFor(() => !isConfirmDialogOpened(toolbox));
  ok(true, "Clicking the close button does close the tooltip");

  info(
    "Check that the tooltip closes when there's no more reason to display it"
  );
  // Open the tooltip again
  EventUtils.synthesizeKey(" ", { ctrlKey: true });
  await waitFor(() => isConfirmDialogOpened(toolbox));

  // Adding a space will make the input `foo.rab.t `, which we shouldn't try to
  // autocomplete.
  EventUtils.sendString(" ");
  await waitFor(() => !isConfirmDialogOpened(toolbox));
  ok(
    true,
    "The tooltip is now closed since the input doesn't match a getter name"
  );
  info("Check that evaluating the expression closes the tooltip");
  tooltip = await setInputValueForGetterConfirmDialog(toolbox, hud, "foo.rab.");
  EventUtils.sendString("length");
  EventUtils.synthesizeKey("KEY_Enter");
  await waitFor(() => !isConfirmDialogOpened(toolbox));
  await waitFor(() => findEvaluationResultMessage(hud, "3"));
  ok("Expression was evaluated and tooltip was closed");
});
