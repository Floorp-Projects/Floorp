/* vim: set ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Tests that selecting a node with the mouse (by clicking on the line) focuses
// the first focusable element in the corresponding MarkupContainer so that the
// keyboard can be used immediately.

const TEST_URL = `data:text/html;charset=utf8,
                  <div class='test-class'></div>Text node`;

add_task(async function() {
  const {inspector} = await openInspectorForURL(TEST_URL);
  const {walker} = inspector;

  info("Select the test node to have the 2 test containers visible");
  await selectNode("div", inspector);

  const divFront = await walker.querySelector(walker.rootNode, "div");
  const textFront = await walker.nextSibling(divFront);

  info("Click on the MarkupContainer element for the text node");
  await clickContainer(textFront, inspector);
  is(inspector.markup.doc.activeElement,
     getContainerForNodeFront(textFront, inspector).editor.value,
     "The currently focused element is the node's text content");

  info("Click on the MarkupContainer element for the <div> node");
  await clickContainer(divFront, inspector);
  is(inspector.markup.doc.activeElement,
     getContainerForNodeFront(divFront, inspector).editor.tag,
     "The currently focused element is the div's tagname");

  info("Click on the test-class attribute, to make sure it gets focused");
  const editor = getContainerForNodeFront(divFront, inspector).editor;
  const attributeEditor = editor.attrElements.get("class")
                                           .querySelector(".editable");

  const onFocus = once(attributeEditor, "focus");
  EventUtils.synthesizeMouseAtCenter(attributeEditor, {type: "mousedown"},
    inspector.markup.doc.defaultView);
  EventUtils.synthesizeMouseAtCenter(attributeEditor, {type: "mouseup"},
    inspector.markup.doc.defaultView);
  await onFocus;

  is(inspector.markup.doc.activeElement, attributeEditor,
     "The currently focused element is the div's class attribute");
});
