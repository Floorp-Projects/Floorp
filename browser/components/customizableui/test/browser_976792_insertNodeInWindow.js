/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const kToolbarName = "test-insertNodeInWindow-placements-toolbar";
const kTestWidgetPrefix = "test-widget-for-insertNodeInWindow-placements-";


/*
Tries to replicate the situation of having a placement list like this:

exists-1,trying-to-insert-this,doesn't-exist,exists-2
*/
add_task(function*() {
  let testWidgetExists = [true, false, false, true];
  let widgetIds = [];
  for (let i = 0; i < testWidgetExists.length; i++) {
    let id = kTestWidgetPrefix + i;
    widgetIds.push(id);
    if (testWidgetExists[i]) {
      let spec = {id: id, type: "button", removable: true, label: "test", tooltiptext: "" + i};
      CustomizableUI.createWidget(spec);
    }
  }

  let toolbarNode = createToolbarWithPlacements(kToolbarName, widgetIds);
  assertAreaPlacements(kToolbarName, widgetIds);

  let btnId = kTestWidgetPrefix + 1;
  let btn = createDummyXULButton(btnId, "test");
  CustomizableUI.ensureWidgetPlacedInWindow(btnId, window);

  is(btn.parentNode.id, kToolbarName, "New XUL widget should be placed inside new toolbar");

  is(btn.previousSibling.id, toolbarNode.firstChild.id,
     "insertNodeInWindow should have placed new XUL widget in correct place in DOM according to placements");

  widgetIds.forEach(id => CustomizableUI.destroyWidget(id));
  btn.remove();
  removeCustomToolbars();
  yield resetCustomization();
});


/*
Tests nodes get placed inside the toolbar's overflow as expected. Replicates a
situation similar to:

exists-1,exists-2,overflow-1,trying-to-insert-this,overflow-2
*/
add_task(function*() {
  let navbar = document.getElementById(CustomizableUI.AREA_NAVBAR);

  let widgetIds = [];
  for (let i = 0; i < 5; i++) {
    let id = kTestWidgetPrefix + i;
    widgetIds.push(id);
    let spec = {id: id, type: "button", removable: true, label: "insertNodeInWindow test", tooltiptext: "" + i};
    CustomizableUI.createWidget(spec);
    CustomizableUI.addWidgetToArea(id, "nav-bar");
  }

  for (let id of widgetIds) {
    document.getElementById(id).style.minWidth = "200px";
  }

  let originalWindowWidth = window.outerWidth;
  window.resizeTo(400, window.outerHeight);
  yield waitForCondition(() => navbar.hasAttribute("overflowing"));

  let testWidgetId = kTestWidgetPrefix + 3;

  CustomizableUI.destroyWidget(testWidgetId);

  let btn = createDummyXULButton(testWidgetId, "test");
  CustomizableUI.ensureWidgetPlacedInWindow(testWidgetId, window);

  is(btn.parentNode.id, navbar.overflowable._list.id, "New XUL widget should be placed inside overflow of toolbar");
  is(btn.previousSibling.id, kTestWidgetPrefix + 2,
     "insertNodeInWindow should have placed new XUL widget in correct place in DOM according to placements");
  is(btn.nextSibling.id, kTestWidgetPrefix + 4,
     "insertNodeInWindow should have placed new XUL widget in correct place in DOM according to placements");

  window.resizeTo(originalWindowWidth, window.outerHeight);

  widgetIds.forEach(id => CustomizableUI.destroyWidget(id));
  CustomizableUI.removeWidgetFromArea(btn.id, kToolbarName);
  btn.remove();
  yield resetCustomization();
  yield waitForCondition(() => !navbar.hasAttribute("overflowing"));
});


/*
Tests nodes get placed inside the toolbar's overflow as expected. Replicates a
placements situation similar to:

exists-1,exists-2,overflow-1,doesn't-exist,trying-to-insert-this,overflow-2
*/
add_task(function*() {
  let navbar = document.getElementById(CustomizableUI.AREA_NAVBAR);

  let widgetIds = [];
  for (let i = 0; i < 5; i++) {
    let id = kTestWidgetPrefix + i;
    widgetIds.push(id);
    let spec = {id: id, type: "button", removable: true, label: "insertNodeInWindow test", tooltiptext: "" + i};
    CustomizableUI.createWidget(spec);
    CustomizableUI.addWidgetToArea(id, "nav-bar");
  }

  for (let id of widgetIds) {
    document.getElementById(id).style.minWidth = "200px";
  }

  let originalWindowWidth = window.outerWidth;
  window.resizeTo(400, window.outerHeight);
  yield waitForCondition(() => navbar.hasAttribute("overflowing"));

  let testWidgetId = kTestWidgetPrefix + 3;

  CustomizableUI.destroyWidget(kTestWidgetPrefix + 2);
  CustomizableUI.destroyWidget(testWidgetId);

  let btn = createDummyXULButton(testWidgetId, "test");
  CustomizableUI.ensureWidgetPlacedInWindow(testWidgetId, window);

  is(btn.parentNode.id, navbar.overflowable._list.id, "New XUL widget should be placed inside overflow of toolbar");
  is(btn.previousSibling.id, kTestWidgetPrefix + 1,
     "insertNodeInWindow should have placed new XUL widget in correct place in DOM according to placements");
  is(btn.nextSibling.id, kTestWidgetPrefix + 4,
     "insertNodeInWindow should have placed new XUL widget in correct place in DOM according to placements");

  window.resizeTo(originalWindowWidth, window.outerHeight);

  widgetIds.forEach(id => CustomizableUI.destroyWidget(id));
  CustomizableUI.removeWidgetFromArea(btn.id, kToolbarName);
  btn.remove();
  yield resetCustomization();
  yield waitForCondition(() => !navbar.hasAttribute("overflowing"));
});


/*
Tests nodes get placed inside the toolbar's overflow as expected. Replicates a
placements situation similar to:

exists-1,exists-2,overflow-1,doesn't-exist,trying-to-insert-this,doesn't-exist
*/
add_task(function*() {
  let navbar = document.getElementById(CustomizableUI.AREA_NAVBAR);

  let widgetIds = [];
  for (let i = 0; i < 5; i++) {
    let id = kTestWidgetPrefix + i;
    widgetIds.push(id);
    let spec = {id: id, type: "button", removable: true, label: "insertNodeInWindow test", tooltiptext: "" + i};
    CustomizableUI.createWidget(spec);
    CustomizableUI.addWidgetToArea(id, "nav-bar");
  }

  for (let id of widgetIds) {
    document.getElementById(id).style.minWidth = "200px";
  }

  let originalWindowWidth = window.outerWidth;
  window.resizeTo(400, window.outerHeight);
  yield waitForCondition(() => navbar.hasAttribute("overflowing"));

  let testWidgetId = kTestWidgetPrefix + 3;

  CustomizableUI.destroyWidget(kTestWidgetPrefix + 2);
  CustomizableUI.destroyWidget(testWidgetId);
  CustomizableUI.destroyWidget(kTestWidgetPrefix + 4);

  let btn = createDummyXULButton(testWidgetId, "test");
  CustomizableUI.ensureWidgetPlacedInWindow(testWidgetId, window);

  is(btn.parentNode.id, navbar.overflowable._list.id, "New XUL widget should be placed inside overflow of toolbar");
  is(btn.previousSibling.id, kTestWidgetPrefix + 1,
     "insertNodeInWindow should have placed new XUL widget in correct place in DOM according to placements");
  is(btn.nextSibling, null,
     "insertNodeInWindow should have placed new XUL widget in correct place in DOM according to placements");

  window.resizeTo(originalWindowWidth, window.outerHeight);

  widgetIds.forEach(id => CustomizableUI.destroyWidget(id));
  CustomizableUI.removeWidgetFromArea(btn.id, kToolbarName);
  btn.remove();
  yield resetCustomization();
  yield waitForCondition(() => !navbar.hasAttribute("overflowing"));
});


/*
Tests nodes get placed inside the toolbar's overflow as expected. Replicates a
placements situation similar to:

exists-1,exists-2,overflow-1,can't-overflow,trying-to-insert-this,overflow-2
*/
add_task(function*() {
  let navbar = document.getElementById(CustomizableUI.AREA_NAVBAR);

  let widgetIds = [];
  for (let i = 5; i >= 0; i--) {
    let id = kTestWidgetPrefix + i;
    widgetIds.push(id);
    let spec = {id: id, type: "button", removable: true, label: "insertNodeInWindow test", tooltiptext: "" + i};
    CustomizableUI.createWidget(spec);
    CustomizableUI.addWidgetToArea(id, "nav-bar", 0);
  }

  for (let i = 10; i < 15; i++) {
    let id = kTestWidgetPrefix + i;
    widgetIds.push(id);
    let spec = {id: id, type: "button", removable: true, label: "insertNodeInWindow test", tooltiptext: "" + i};
    CustomizableUI.createWidget(spec);
    CustomizableUI.addWidgetToArea(id, "nav-bar");
  }

  for (let id of widgetIds) {
    document.getElementById(id).style.minWidth = "200px";
  }

  let originalWindowWidth = window.outerWidth;
  window.resizeTo(400, window.outerHeight);
  yield waitForCondition(() => navbar.hasAttribute("overflowing"));

  // Find last widget that doesn't allow overflowing
  let nonOverflowing = navbar.customizationTarget.lastChild;
  is(nonOverflowing.getAttribute("overflows"), "false", "Last child is expected to not allow overflowing");
  isnot(nonOverflowing.getAttribute("skipintoolbarset"), "true", "Last child is expected to not be skipintoolbarset");

  let testWidgetId = kTestWidgetPrefix + 10;
  CustomizableUI.destroyWidget(testWidgetId);

  let btn = createDummyXULButton(testWidgetId, "test");
  CustomizableUI.ensureWidgetPlacedInWindow(testWidgetId, window);

  is(btn.parentNode.id, navbar.overflowable._list.id, "New XUL widget should be placed inside overflow of toolbar");
  is(btn.nextSibling.id, kTestWidgetPrefix + 11,
     "insertNodeInWindow should have placed new XUL widget in correct place in DOM according to placements");

  window.resizeTo(originalWindowWidth, window.outerHeight);

  widgetIds.forEach(id => CustomizableUI.destroyWidget(id));
  CustomizableUI.removeWidgetFromArea(btn.id, kToolbarName);
  btn.remove();
  yield resetCustomization();
  yield waitForCondition(() => !navbar.hasAttribute("overflowing"));
});


/*
Tests nodes get placed inside the toolbar's overflow as expected. Replicates a
placements situation similar to:

exists-1,exists-2,overflow-1,trying-to-insert-this,can't-overflow,overflow-2
*/
add_task(function*() {
  let widgetIds = [];
  let missingId = 2;
  let nonOverflowableId = 3;
  for (let i = 0; i < 5; i++) {
    let id = kTestWidgetPrefix + i;
    widgetIds.push(id);
    if (i != missingId) {
      // Setting min-width to make the overflow state not depend on styling of the button and/or
      // screen width
      let spec = {id: id, type: "button", removable: true, label: "test", tooltiptext: "" + i,
                  onCreated: function(node) {
                    node.style.minWidth = "200px";
                    if (id == (kTestWidgetPrefix + nonOverflowableId)) {
                      node.setAttribute("overflows", false);
                    }
                 }};
      info("Creating: " + id);
      CustomizableUI.createWidget(spec);
    }
  }

  let toolbarNode = createOverflowableToolbarWithPlacements(kToolbarName, widgetIds);
  assertAreaPlacements(kToolbarName, widgetIds);
  ok(!toolbarNode.hasAttribute("overflowing"), "Toolbar shouldn't overflow to start with.");

  let originalWindowWidth = window.outerWidth;
  window.resizeTo(400, window.outerHeight);
  yield waitForCondition(() => toolbarNode.hasAttribute("overflowing"));
  ok(toolbarNode.hasAttribute("overflowing"), "Should have an overflowing toolbar.");

  let btnId = kTestWidgetPrefix + missingId;
  let btn = createDummyXULButton(btnId, "test");
  CustomizableUI.ensureWidgetPlacedInWindow(btnId, window);

  is(btn.parentNode.id, kToolbarName + "-overflow-list", "New XUL widget should be placed inside new toolbar's overflow");
  is(btn.previousSibling.id, kTestWidgetPrefix + 1,
     "insertNodeInWindow should have placed new XUL widget in correct place in DOM according to placements");
  is(btn.nextSibling.id, kTestWidgetPrefix + 4,
     "insertNodeInWindow should have placed new XUL widget in correct place in DOM according to placements");

  window.resizeTo(originalWindowWidth, window.outerHeight);
  yield waitForCondition(() => !toolbarNode.hasAttribute("overflowing"));

  btn.remove();
  widgetIds.forEach(id => CustomizableUI.destroyWidget(id));
  removeCustomToolbars();
  yield resetCustomization();
});


/*
Tests nodes do *not* get placed in the toolbar's overflow. Replicates a
plcements situation similar to:

exists-1,trying-to-insert-this,exists-2,overflowed-1
*/
add_task(function*() {
  let widgetIds = [];
  let missingId = 1;
  for (let i = 0; i < 5; i++) {
    let id = kTestWidgetPrefix + i;
    widgetIds.push(id);
    if (i != missingId) {
      // Setting min-width to make the overflow state not depend on styling of the button and/or
      // screen width
      let spec = {id: id, type: "button", removable: true, label: "test", tooltiptext: "" + i,
                  onCreated: function(node) { node.style.minWidth = "100px"; }};
      info("Creating: " + id);
      CustomizableUI.createWidget(spec);
    }
  }

  let toolbarNode = createOverflowableToolbarWithPlacements(kToolbarName, widgetIds);
  assertAreaPlacements(kToolbarName, widgetIds);
  ok(!toolbarNode.hasAttribute("overflowing"), "Toolbar shouldn't overflow to start with.");

  let originalWindowWidth = window.outerWidth;
  window.resizeTo(400, window.outerHeight);
  yield waitForCondition(() => toolbarNode.hasAttribute("overflowing"));
  ok(toolbarNode.hasAttribute("overflowing"), "Should have an overflowing toolbar.");

  let btnId = kTestWidgetPrefix + missingId;
  let btn = createDummyXULButton(btnId, "test");
  CustomizableUI.ensureWidgetPlacedInWindow(btnId, window);

  is(btn.parentNode.id, kToolbarName + "-target", "New XUL widget should be placed inside new toolbar");

  window.resizeTo(originalWindowWidth, window.outerHeight);
  yield waitForCondition(() => !toolbarNode.hasAttribute("overflowing"));

  btn.remove();
  widgetIds.forEach(id => CustomizableUI.destroyWidget(id));
  removeCustomToolbars();
  yield resetCustomization();
});


/*
Tests inserting a node onto the end of an overflowing toolbar *doesn't* put it in
the overflow list when the widget disallows overflowing. ie:

exists-1,exists-2,overflows-1,trying-to-insert-this

Where trying-to-insert-this has overflows=false
*/
add_task(function*() {
  let widgetIds = [];
  let missingId = 3;
  for (let i = 0; i < 5; i++) {
    let id = kTestWidgetPrefix + i;
    widgetIds.push(id);
    if (i != missingId) {
      // Setting min-width to make the overflow state not depend on styling of the button and/or
      // screen width
      let spec = {id: id, type: "button", removable: true, label: "test", tooltiptext: "" + i,
                  onCreated: function(node) { node.style.minWidth = "200px"; }};
      info("Creating: " + id);
      CustomizableUI.createWidget(spec);
    }
  }

  let toolbarNode = createOverflowableToolbarWithPlacements(kToolbarName, widgetIds);
  assertAreaPlacements(kToolbarName, widgetIds);
  ok(!toolbarNode.hasAttribute("overflowing"), "Toolbar shouldn't overflow to start with.");

  let originalWindowWidth = window.outerWidth;
  window.resizeTo(400, window.outerHeight);
  yield waitForCondition(() => toolbarNode.hasAttribute("overflowing"));
  ok(toolbarNode.hasAttribute("overflowing"), "Should have an overflowing toolbar.");

  let btnId = kTestWidgetPrefix + missingId;
  let btn = createDummyXULButton(btnId, "test");
  btn.setAttribute("overflows", false);
  CustomizableUI.ensureWidgetPlacedInWindow(btnId, window);

  is(btn.parentNode.id, kToolbarName + "-target", "New XUL widget should be placed inside new toolbar");
  is(btn.nextSibling, null,
     "insertNodeInWindow should have placed new XUL widget in correct place in DOM according to placements");

  window.resizeTo(originalWindowWidth, window.outerHeight);
  yield waitForCondition(() => !toolbarNode.hasAttribute("overflowing"));

  btn.remove();
  widgetIds.forEach(id => CustomizableUI.destroyWidget(id));
  removeCustomToolbars();
  yield resetCustomization();
});


add_task(function* asyncCleanUp() {
  yield resetCustomization();
});
