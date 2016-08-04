/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Tests keyboard navigation of devtools tabbar.

const TEST_URL =
  "data:text/html;charset=utf8,test page for toolbar keyboard navigation";

function containsFocus(aDoc, aElm) {
  let elm = aDoc.activeElement;
  while (elm) {
    if (elm === aElm) { return true; }
    elm = elm.parentNode;
  }
  return false;
}

add_task(function* () {
  info("Create a test tab and open the toolbox");
  let toolbox = yield openNewTabAndToolbox(TEST_URL, "webconsole");
  let doc = toolbox.doc;

  let toolbar = doc.querySelector(".devtools-tabbar");
  let toolbarControls = [...toolbar.querySelectorAll(
    ".devtools-tab, button")].filter(elm =>
      !elm.hidden && doc.defaultView.getComputedStyle(elm).getPropertyValue(
        "display") !== "none");

  // Put the keyboard focus onto the first toolbar control.
  toolbarControls[0].focus();
  ok(containsFocus(doc, toolbar), "Focus is within the toolbar");

  // Move the focus away from toolbar to a next focusable element.
  EventUtils.synthesizeKey("VK_TAB", {});
  ok(!containsFocus(doc, toolbar), "Focus is outside of the toolbar");

  // Move the focus back to the toolbar.
  EventUtils.synthesizeKey("VK_TAB", { shiftKey: true });
  ok(containsFocus(doc, toolbar), "Focus is within the toolbar again");

  // Move through the toolbar forward using the right arrow key.
  for (let i = 0; i < toolbarControls.length; ++i) {
    is(doc.activeElement.id, toolbarControls[i].id, "New control is focused");
    if (i < toolbarControls.length - 1) {
      EventUtils.synthesizeKey("VK_RIGHT", {});
    }
  }

  // Move the focus away from toolbar to a next focusable element.
  EventUtils.synthesizeKey("VK_TAB", {});
  ok(!containsFocus(doc, toolbar), "Focus is outside of the toolbar");

  // Move the focus back to the toolbar.
  EventUtils.synthesizeKey("VK_TAB", { shiftKey: true });
  ok(containsFocus(doc, toolbar), "Focus is within the toolbar again");

  // Move through the toolbar backward using the left arrow key.
  for (let i = toolbarControls.length - 1; i >= 0; --i) {
    is(doc.activeElement.id, toolbarControls[i].id, "New control is focused");
    if (i > 0) { EventUtils.synthesizeKey("VK_LEFT", {}); }
  }

  // Move focus to the 3rd (non-first) toolbar control.
  let expectedFocusedControl = toolbarControls[2];
  EventUtils.synthesizeKey("VK_RIGHT", {});
  EventUtils.synthesizeKey("VK_RIGHT", {});
  is(doc.activeElement.id, expectedFocusedControl.id, "New control is focused");

  // Move the focus away from toolbar to a next focusable element.
  EventUtils.synthesizeKey("VK_TAB", {});
  ok(!containsFocus(doc, toolbar), "Focus is outside of the toolbar");

  // Move the focus back to the toolbar, ensure we land on the last active
  // descendant control.
  EventUtils.synthesizeKey("VK_TAB", { shiftKey: true });
  is(doc.activeElement.id, expectedFocusedControl.id, "New control is focused");
});
