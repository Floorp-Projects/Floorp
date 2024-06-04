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

add_task(async function () {
  await addTab("data:text/html;charset=utf-8," + encodeURIComponent(TEST_URI));
  const { inspector, view } = await openRuleView();
  await selectNode("#testid", inspector);

  let elementRuleEditor = getRuleViewRuleEditor(view, 1);
  is(
    elementRuleEditor.rule.textProps.length,
    1,
    "Sanity check, the rule has 1 property at the beginning of the test."
  );

  info("Creating a new property but don't commit…");
  const textProp = await addProperty(view, 1, "color", "red", {
    commitValueWith: null,
    blurNewProperty: false,
  });

  info("The autocomplete popup should be displayed, hit Escape to hide it");
  await waitFor(() => view.popup && view.popup.isOpen);
  ok(true, "Popup was opened");
  const onPopupClosed = once(view.popup, "popup-closed");
  EventUtils.synthesizeKey("VK_ESCAPE", {}, view.styleWindow);
  await onPopupClosed;
  ok(true, "Popup was closed");

  is(
    view.styleDocument.activeElement,
    inplaceEditor(textProp.editor.valueSpan).input,
    "The autocomplete was closed, but focus is still on the new property value"
  );

  is(
    elementRuleEditor.rule.textProps.length,
    2,
    "The new property is still displayed"
  );

  info("Hit escape to remove the property");
  const onRuleViewChanged = view.once("ruleview-changed");
  EventUtils.synthesizeKey("VK_ESCAPE", {}, view.styleWindow);
  await onRuleViewChanged;

  is(
    view.styleDocument.activeElement,
    view.styleDocument.body,
    "Correct element has focus"
  );

  elementRuleEditor = getRuleViewRuleEditor(view, 1);
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
