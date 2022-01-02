/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Tests adding a new property and escapes the new empty property value editor.

const TEST_URI = `
  <style type='text/css'>
    #testid {
      background-color: blue;
    }
  </style>
  <div id='testid'>Styled Node</div>
`;

add_task(async function() {
  await addTab("data:text/html;charset=utf-8," + encodeURIComponent(TEST_URI));
  const { inspector, view } = await openRuleView();
  await selectNode("#testid", inspector);

  info("Test creating a new property and escaping");
  await addProperty(view, 1, "color", "red", "VK_ESCAPE", false);

  is(
    view.styleDocument.activeElement,
    view.styleDocument.body,
    "Correct element has focus"
  );

  const elementRuleEditor = getRuleViewRuleEditor(view, 1);
  is(
    elementRuleEditor.rule.textProps.length,
    1,
    "Removed the new text property."
  );
  is(
    elementRuleEditor.propertyList.children.length,
    1,
    "Removed the property editor."
  );
});
