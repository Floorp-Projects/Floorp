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
    if (elm === aElm) {
      return true;
    }
    elm = elm.parentNode;
  }
  return false;
}

add_task(async function() {
  info("Create a test tab and open the toolbox");
  const toolbox = await openNewTabAndToolbox(TEST_URL, "webconsole");
  const doc = toolbox.doc;

  const toolbar = doc.querySelector(".devtools-tabbar");
  const toolbarControls = [...toolbar.querySelectorAll(
    ".devtools-tab, button")].filter(elm =>
      !elm.hidden && doc.defaultView.getComputedStyle(elm).getPropertyValue(
        "display") !== "none");

  // Put the keyboard focus onto the first toolbar control.
  toolbarControls[0].focus();
  ok(containsFocus(doc, toolbar), "Focus is within the toolbar");

  // Move the focus away from toolbar to a next focusable element.
  EventUtils.synthesizeKey("KEY_Tab");
  ok(!containsFocus(doc, toolbar), "Focus is outside of the toolbar");

  // Move the focus back to the toolbar.
  EventUtils.synthesizeKey("KEY_Tab", {shiftKey: true});
  ok(containsFocus(doc, toolbar), "Focus is within the toolbar again");

  // Move through the toolbar forward using the right arrow key.
  for (let i = 0; i < toolbarControls.length; ++i) {
    is(doc.activeElement.id, toolbarControls[i].id, "New control is focused");
    if (i < toolbarControls.length - 1) {
      EventUtils.synthesizeKey("KEY_ArrowRight");
    }
  }

  // Move the focus away from toolbar to a next focusable element.
  EventUtils.synthesizeKey("KEY_Tab");
  ok(!containsFocus(doc, toolbar), "Focus is outside of the toolbar");

  // Move the focus back to the toolbar.
  EventUtils.synthesizeKey("KEY_Tab", {shiftKey: true});
  ok(containsFocus(doc, toolbar), "Focus is within the toolbar again");

  // Move through the toolbar backward using the left arrow key.
  for (let i = toolbarControls.length - 1; i >= 0; --i) {
    is(doc.activeElement.id, toolbarControls[i].id, "New control is focused");
    if (i > 0) {
      EventUtils.synthesizeKey("KEY_ArrowLeft");
    }
  }

  // Move focus to the 3rd (non-first) toolbar control.
  const expectedFocusedControl = toolbarControls[2];
  EventUtils.synthesizeKey("KEY_ArrowRight");
  EventUtils.synthesizeKey("KEY_ArrowRight");
  is(doc.activeElement.id, expectedFocusedControl.id, "New control is focused");

  // Move the focus away from toolbar to a next focusable element.
  EventUtils.synthesizeKey("KEY_Tab");
  ok(!containsFocus(doc, toolbar), "Focus is outside of the toolbar");

  // Move the focus back to the toolbar, ensure we land on the last active
  // descendant control.
  EventUtils.synthesizeKey("KEY_Tab", {shiftKey: true});
  is(doc.activeElement.id, expectedFocusedControl.id, "New control is focused");
});

// Test that moving the focus of tab button and selecting it.
add_task(async function() {
  info("Create a test tab and open the toolbox");
  const toolbox = await openNewTabAndToolbox(TEST_URL, "inspector");
  const doc = toolbox.doc;

  const toolbar = doc.querySelector(".toolbox-tabs");
  const tabButtons = toolbar.querySelectorAll(".devtools-tab, button");
  const win = tabButtons[0].ownerDocument.defaultView;

  // Put the keyboard focus onto the first tab button.
  tabButtons[0].focus();
  ok(containsFocus(doc, toolbar), "Focus is within the toolbox");
  is(doc.activeElement.id, tabButtons[0].id, "First tab button is focused.");

  // Move the focused tab and select it by using enter key.
  let onKeyEvent = once(win, "keydown");
  EventUtils.synthesizeKey("KEY_ArrowRight");
  await onKeyEvent;

  let onceSelected = toolbox.once("webconsole-selected");
  EventUtils.synthesizeKey("Enter");
  await onceSelected;
  ok(doc.activeElement.id, tabButtons[1].id, "Changed tab button is focused.");

  // Webconsole steal the focus from button after sending "webconsole-selected"
  // event.
  tabButtons[1].focus();

  // Return the focused tab with space key.
  onKeyEvent = once(win, "keydown");
  EventUtils.synthesizeKey("KEY_ArrowLeft");
  await onKeyEvent;

  onceSelected = toolbox.once("inspector-selected");
  EventUtils.synthesizeKey(" ");
  await onceSelected;

  ok(doc.activeElement.id, tabButtons[0].id, "Changed tab button is focused.");
});
