/* vim: set ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

requestLongerTimeout(2);

// Tests tabbing through attributes on a node

const TEST_URL = "data:text/html;charset=utf8,<div id='test' a b c d e></div>";

add_task(async function() {
  const {inspector} = await openInspectorForURL(TEST_URL);

  info("Focusing the tag editor of the test element");
  const {editor} = await focusNode("div", inspector);
  editor.tag.focus();

  info("Pressing tab and expecting to focus the ID attribute, always first");
  EventUtils.sendKey("tab", inspector.panelWin);
  checkFocusedAttribute("id");

  info("Hit enter to turn the attribute to edit mode");
  EventUtils.sendKey("return", inspector.panelWin);
  checkFocusedAttribute("id", true);

  // Check the order of the other attributes in the DOM to the check they appear
  // correctly in the markup-view
  const attributes = (await getAttributesFromEditor("div", inspector)).slice(1);

  info("Tabbing forward through attributes in edit mode");
  for (const attribute of attributes) {
    collapseSelectionAndTab(inspector);
    checkFocusedAttribute(attribute, true);
  }

  info("Tabbing backward through attributes in edit mode");

  // Just reverse the attributes other than id and remove the first one since
  // it's already focused now.
  const reverseAttributes = attributes.reverse();
  reverseAttributes.shift();

  for (const attribute of reverseAttributes) {
    collapseSelectionAndShiftTab(inspector);
    checkFocusedAttribute(attribute, true);
  }
});
