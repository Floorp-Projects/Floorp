/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test adding an invalid property.

const TEST_URI = `
  <style type='text/css'>
    #testid {
      background-color: blue;
    }
    .testclass {
      background-color: green;
    }
  </style>
  <div id='testid' class='testclass'>Styled Node</div>
`;

add_task(async function() {
  await addTab("data:text/html;charset=utf-8," + encodeURIComponent(TEST_URI));
  const { inspector, view } = await openRuleView();
  await selectNode("#testid", inspector);

  info("Test creating a new property");
  const textProp = await addProperty(view, 0, "background-color", "#XYZ");

  is(textProp.value, "#XYZ", "Text prop should have been changed.");
  is(textProp.overridden, true, "Property should be overridden");
  is(textProp.editor.isValid(), false, "#XYZ should not be a valid entry");
});
