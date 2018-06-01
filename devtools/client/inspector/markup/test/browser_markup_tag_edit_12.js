/* vim: set ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Tests that focus position is correct when tabbing through and editing
// attributes.

const TEST_URL = "data:text/html;charset=utf8," +
                 "<div id='attr' a='1' b='2' c='3'></div>" +
                 "<div id='delattr' tobeinvalid='1' last='2'></div>";

add_task(async function() {
  const {inspector} = await openInspectorForURL(TEST_URL);

  await testAttributeEditing(inspector);
  await testAttributeDeletion(inspector);
});

async function testAttributeEditing(inspector) {
  info("Testing focus position after attribute editing");

  info("Setting the first non-id attribute in edit mode");
  // focuses id
  await activateFirstAttribute("#attr", inspector);
  // focuses the first attr after id
  collapseSelectionAndTab(inspector);

  const attrs = await getAttributesFromEditor("#attr", inspector);

  info("Editing this attribute, keeping the same name, " +
       "and tabbing to the next");
  await editAttributeAndTab(attrs[1] + '="99"', inspector);
  checkFocusedAttribute(attrs[2], true);

  info("Editing the new focused attribute, keeping the name, " +
       "and tabbing to the previous");
  await editAttributeAndTab(attrs[2] + '="99"', inspector, true);
  checkFocusedAttribute(attrs[1], true);

  info("Editing attribute name, changes attribute order");
  await editAttributeAndTab("d='4'", inspector);
  checkFocusedAttribute("id", true);

  // Escape of the currently focused field for the next test
  EventUtils.sendKey("escape", inspector.panelWin);
}

async function testAttributeDeletion(inspector) {
  info("Testing focus position after attribute deletion");

  info("Setting the first non-id attribute in edit mode");
  // focuses id
  await activateFirstAttribute("#delattr", inspector);
  // focuses the first attr after id
  collapseSelectionAndTab(inspector);

  const attrs = await getAttributesFromEditor("#delattr", inspector);

  info("Entering an invalid attribute to delete the attribute");
  await editAttributeAndTab('"', inspector);
  checkFocusedAttribute(attrs[2], true);

  info("Deleting the last attribute");
  await editAttributeAndTab(" ", inspector);

  // Check we're on the newattr element
  const focusedAttr = Services.focus.focusedElement;
  ok(focusedAttr.classList.contains("styleinspector-propertyeditor"),
     "in newattr");
  is(focusedAttr.tagName, "textarea", "newattr is active");
}

async function editAttributeAndTab(newValue, inspector, goPrevious) {
  const onEditMutation = inspector.markup.once("refocusedonedit");
  inspector.markup.doc.activeElement.value = newValue;
  if (goPrevious) {
    EventUtils.synthesizeKey("VK_TAB", { shiftKey: true },
      inspector.panelWin);
  } else {
    EventUtils.sendKey("tab", inspector.panelWin);
  }
  await onEditMutation;
}

/**
 * Given a markup container, focus and turn in edit mode its first attribute
 * field.
 */
async function activateFirstAttribute(container, inspector) {
  const {editor} = await focusNode(container, inspector);
  editor.tag.focus();

  // Go to "id" attribute and trigger edit mode.
  EventUtils.sendKey("tab", inspector.panelWin);
  EventUtils.sendKey("return", inspector.panelWin);
}
