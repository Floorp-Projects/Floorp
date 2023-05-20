/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test adding properties that are invalid identifiers.

const TEST_URI = "<div id='testid'>Styled Node</div>";
const TEST_DATA = [
  { name: "1", value: "100" },
  { name: "-1", value: "100" },
  { name: "1a", value: "100" },
  { name: "-11a", value: "100" },
];

add_task(async function () {
  await addTab("data:text/html;charset=utf-8," + encodeURIComponent(TEST_URI));
  const { inspector, view } = await openRuleView();
  await selectNode("#testid", inspector);

  for (const { name, value } of TEST_DATA) {
    info(`Test creating a new property ${name}: ${value}`);
    const declaration = await addProperty(view, 0, name, value);

    is(declaration.name, name, "Property name should have been changed.");
    is(declaration.value, value, "Property value should have been changed.");
    is(
      declaration.editor.isValid(),
      false,
      "The declaration should be invalid."
    );
  }
});
