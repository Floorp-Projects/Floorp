/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Tests the shortcut key for the suggest completion popup.

const TEST_URI = "<h1 style='colo: lim'>Header</h1>";
const TEST_SHORTCUTS = [
  {
    key: " ",
    modifiers: { ctrlKey: true },
  },
  {
    key: "VK_DOWN",
    modifiers: {},
  },
];

add_task(async function () {
  for (const shortcut of TEST_SHORTCUTS) {
    info(
      "Start to test for the shortcut " +
        `key: "${shortcut.key}" modifiers: ${Object.keys(shortcut.modifiers)}`
    );

    const tab = await addTab(
      "data:text/html;charset=utf-8," + encodeURIComponent(TEST_URI)
    );
    const { inspector, view } = await openRuleView();

    info("Selecting the test node");
    await selectNode("h1", inspector);

    const prop = getTextProperty(view, 0, { colo: "lim" });

    info("Test with css property name field");
    const nameEditor = await focusEditableField(view, prop.editor.nameSpan);
    await testCompletion(shortcut, view, nameEditor, "color");

    info("Test with css property value field");
    const valueEditor = inplaceEditor(view.styleDocument.activeElement);
    await testCompletion(shortcut, view, valueEditor, "lime");

    await removeTab(tab);
  }
});

async function testCompletion(shortcut, view, editor, expectedValue) {
  const spanEl = editor.elt;

  info("Move cursor to the end");
  EventUtils.synthesizeKey("VK_RIGHT", {}, view.styleWindow);
  await waitUntil(
    () => editor.input.selectionStart === editor.input.selectionEnd
  );

  info("Check whether the popup opens after sending the shortcut key");
  const onPopupOpened = once(view.popup, "popup-opened");
  EventUtils.synthesizeKey(shortcut.key, shortcut.modifiers, view.styleWindow);
  await onPopupOpened;
  ok(view.popup.isOpen, "The popup opened correctly");

  info("Commit the suggestion");
  const onChanged = view.once("ruleview-changed");
  const onPopupClosed = once(view.popup, "popup-closed");
  EventUtils.synthesizeKey("VK_TAB", {}, view.styleWindow);
  await Promise.all([onChanged, onPopupClosed]);
  is(spanEl.textContent, expectedValue, "The value is set correctly");
}
