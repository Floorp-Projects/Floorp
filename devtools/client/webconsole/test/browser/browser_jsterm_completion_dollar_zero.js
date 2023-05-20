/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

// Tests that code completion works properly on $0.

"use strict";

const TEST_URI = `data:text/html;charset=utf-8,<!DOCTYPE html>
<head>
  <title>$0 completion test</title>
</head>
<body>
  <div>
    <h1>$0 completion test</h1>
    <p>This is some example text</p>
  </div>
</body>`;

add_task(async function () {
  const toolbox = await openNewTabAndToolbox(TEST_URI, "inspector");
  await selectNodeWithPicker(toolbox, "h1");

  info("Picker mode stopped, <h1> selected, now switching to the console");
  const hud = await openConsole();
  const { jsterm } = hud;

  await clearOutput(hud);

  const { autocompletePopup } = jsterm;

  await setInputValueForAutocompletion(hud, "$0.");
  ok(
    hasPopupLabel(autocompletePopup, "attributes"),
    "autocomplete popup has expected items"
  );
  is(autocompletePopup.isOpen, true, "autocomplete popup is open");

  await setInputValueForAutocompletion(hud, "$0.attributes.");
  is(autocompletePopup.isOpen, true, "autocomplete popup is open");
  ok(
    hasPopupLabel(autocompletePopup, "getNamedItem"),
    "autocomplete popup has expected items"
  );

  info("Close autocomplete popup");
  await closeAutocompletePopup(hud);
});
