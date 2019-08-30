/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Tests the suggest completion popup behavior of CSS property field.

const TEST_URI = "<h1 style='color: lime'>Header</h1>";

add_task(async function() {
  await addTab("data:text/html;charset=utf-8," + encodeURIComponent(TEST_URI));
  const { inspector, view } = await openRuleView();

  info("Selecting the test node");
  await selectNode("h1", inspector);

  const rule = getRuleViewRuleEditor(view, 0).rule;
  const prop = rule.textProps[0];

  info("Test with css property value field");
  await testCompletion(view, prop.editor.valueSpan, true);

  info("Test with css property name field");
  await testCompletion(view, prop.editor.nameSpan, false);
});

async function testCompletion(view, target, isExpectedOpenPopup) {
  const editor = await focusEditableField(view, target);

  info(
    "Check the suggest completion popup visibility after clearing the field"
  );

  const onChanged = view.once("ruleview-changed");
  const popupEvent = isExpectedOpenPopup ? "popup-opened" : "popup-closed";
  const onPopupEvent =
    editor.popup.isOpen === isExpectedOpenPopup
      ? Promise.resolve()
      : once(view.popup, popupEvent);
  EventUtils.synthesizeKey("VK_BACK_SPACE", {}, view.styleWindow);

  // Flush the debounce to update the preview text.
  view.debounce.flush();

  await Promise.all([onChanged, onPopupEvent]);
  is(
    editor.popup.isOpen,
    isExpectedOpenPopup,
    "The popup visibility is correct"
  );

  if (editor.popup.isOpen) {
    info("Close the suggest completion popup");
    const onPopupClosed = once(view.popup, "popup-closed");
    EventUtils.synthesizeKey("VK_ESCAPE", {}, view.styleWindow);
    await onPopupClosed;
  }
}
