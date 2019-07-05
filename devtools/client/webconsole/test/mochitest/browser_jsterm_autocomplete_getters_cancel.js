/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test that the confirm dialog can be closed with different actions.

const TEST_URI = `data:text/html;charset=utf-8,
<head>
  <script>
    window.foo = {
      get rab() {
        return {};
      }
    };
  </script>
</head>
<body>Autocomplete popup - invoke getter - close dialog test</body>`;

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
  const target = await TargetFactory.forTab(gBrowser.selectedTab);
  const toolbox = gDevTools.getToolbox(target);

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
  let onConfirmTooltipClosed = waitFor(() => !isConfirmDialogOpened(toolbox));
  EventUtils.synthesizeKey("KEY_Escape");
  await onConfirmTooltipClosed;

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

  info("Check that ArrowLeft closes the confirm tooltip");
  onConfirmTooltipClosed = waitFor(() => !isConfirmDialogOpened(toolbox));
  EventUtils.synthesizeKey("KEY_ArrowLeft");
  await onConfirmTooltipClosed;
}
