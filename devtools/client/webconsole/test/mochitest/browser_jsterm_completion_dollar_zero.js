/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

// Tests that code completion works properly on $0.

"use strict";

const TEST_URI = `data:text/html;charset=utf-8,
<head>
  <title>$0 completion test</title>
</head>
<body>
  <div>
    <h1>$0 completion test</h1>
    <p>This is some example text</p>
  </div>
</body>`;

add_task(async function() {
  const toolbox = await openNewTabAndToolbox(TEST_URI, "inspector");
  await registerTestActor(toolbox.target.client);
  const testActor = await getTestActor(toolbox);
  await selectNodeWithPicker(toolbox, testActor, "h1");

  info("Picker mode stopped, <h1> selected, now switching to the console");
  const hud = await openConsole();
  const { jsterm } = hud;

  hud.ui.clearOutput();

  const { autocompletePopup } = jsterm;

  await setInputValueForAutocompletion(hud, "$0.");
  is(
    getAutocompletePopupLabels(autocompletePopup).includes("attributes"),
    true,
    "autocomplete popup has expected items"
  );
  is(autocompletePopup.isOpen, true, "autocomplete popup is open");

  await setInputValueForAutocompletion(hud, "$0.attributes.");
  is(autocompletePopup.isOpen, true, "autocomplete popup is open");
  is(
    getAutocompletePopupLabels(autocompletePopup).includes("getNamedItem"),
    true,
    "autocomplete popup has expected items"
  );
});

function getAutocompletePopupLabels(autocompletePopup) {
  return autocompletePopup.items.map(i => i.label);
}
