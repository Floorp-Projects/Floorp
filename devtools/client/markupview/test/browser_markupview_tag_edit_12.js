/* vim: set ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Tests that focus position is correct when tabbing through and editing
// attributes.

const TEST_URL = "data:text/html;charset=utf8,<div id='attr' c='3' b='2' a='1'></div><div id='delattr' last='1' tobeinvalid='2'></div>";

add_task(function*() {
  let {inspector} = yield addTab(TEST_URL).then(openInspector);

  yield testAttributeEditing(inspector);
  yield testAttributeDeletion(inspector);
});

function* testAttributeEditing(inspector) {
  info("Testing focus position after attribute editing");

  // Modifying attributes reorders them in the internal representation to move
  // the modified attribute to the end. breadcrumbs.js will update attributes
  // to match original order if you selectNode before modifying attributes.
  // So, hacky workaround for consistency with manual testing.
  // Should be removed after Bug 1093593.
  yield selectNode("#attr", inspector);

  info("Setting the first non-id attribute in edit mode");
  yield activateFirstAttribute("#attr", inspector); // focuses id
  collapseSelectionAndTab(inspector); // focuses the first attr after id

  // Detect the attributes order from the DOM, instead of assuming an order in
  // the test, because the NamedNodeMap returned by element.attributes doesn't
  // guaranty any specific order.
  // Filter out the id attribute as the markup-view places it first anyway.
  let attrs = getNodeAttributesOtherThanId("#attr");

  info("Editing this attribute, keeping the same name, and tabbing to the next");
  yield editAttributeAndTab(attrs[0].name + '="99"', inspector);
  checkFocusedAttribute(attrs[1].name, true);

  info("Editing the new focused attribute, keeping the name, and tabbing to the previous");
  yield editAttributeAndTab(attrs[1].name + '="99"', inspector, true);
  checkFocusedAttribute(attrs[0].name, true);

  info("Editing attribute name, changes attribute order");
  yield editAttributeAndTab("d='4'", inspector);
  checkFocusedAttribute("id", true);

  // Escape of the currently focused field for the next test
  EventUtils.sendKey("escape", inspector.panelWin);
}

function* testAttributeDeletion(inspector) {
  info("Testing focus position after attribute deletion");

  // Modifying attributes reorders them in the internal representation to move
  // the modified attribute to the end. breadcrumbs.js will update attributes
  // to match original order if you selectNode before modifying attributes.
  // So, hacky workaround for consistency with manual testing.
  // Should be removed after Bug 1093593.
  yield selectNode("#delattr", inspector);

  info("Setting the first non-id attribute in edit mode");
  yield activateFirstAttribute("#delattr", inspector); // focuses id
  collapseSelectionAndTab(inspector); // focuses the first attr after id

  // Detect the attributes order from the DOM, instead of assuming an order in
  // the test, because the NamedNodeMap returned by element.attributes doesn't
  // guaranty any specific order.
  // Filter out the id attribute as the markup-view places it first anyway.
  let attrs = getNodeAttributesOtherThanId("#delattr");

  info("Entering an invalid attribute to delete the attribute");
  yield editAttributeAndTab('"', inspector);
  checkFocusedAttribute(attrs[1].name, true);

  info("Deleting the last attribute");
  yield editAttributeAndTab(" ", inspector);

  // Check we're on the newattr element
  let focusedAttr = Services.focus.focusedElement;
  ok(focusedAttr.classList.contains("styleinspector-propertyeditor"), "in newattr");
  is(focusedAttr.tagName, "input", "newattr is active");
}

function* editAttributeAndTab(newValue, inspector, goPrevious) {
  var onEditMutation = inspector.markup.once("refocusedonedit");
  inspector.markup.doc.activeElement.value = newValue;
  if (goPrevious) {
    EventUtils.synthesizeKey("VK_TAB", { shiftKey: true },
      inspector.panelWin);
  } else {
    EventUtils.sendKey("tab", inspector.panelWin);
  }
  yield onEditMutation;
}

/**
 * Given a markup container, focus and turn in edit mode its first attribute
 * field.
 */
function* activateFirstAttribute(container, inspector) {
  let {editor} = yield getContainerForSelector(container, inspector);
  editor.tag.focus();

  // Go to "id" attribute and trigger edit mode.
  EventUtils.sendKey("tab", inspector.panelWin);
  EventUtils.sendKey("return", inspector.panelWin);
}

function getNodeAttributesOtherThanId(selector) {
  return [...getNode(selector).attributes].filter(attr => attr.name !== "id");
}
