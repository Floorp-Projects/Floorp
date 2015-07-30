/* vim: set ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test drag mode's delay, it shouldn't enable dragging before
// GRAB_DELAY = 400 has passed

const TEST_URL = TEST_URL_ROOT + "doc_markup_dragdrop.html";
const GRAB_DELAY = 400;

add_task(function*() {
  let {inspector} = yield addTab(TEST_URL).then(openInspector);

  let el = yield getContainerForSelector("#test", inspector);
  let rect = el.tagLine.getBoundingClientRect();

  info("Simulating mouseDown on #test");
  el._onMouseDown({
    target: el.tagLine,
    pageX: rect.x,
    pageY: rect.y,
    stopPropagation: function() {},
    preventDefault: function() {}
  });

  ok(!el.isDragging, "isDragging should not be set to true immediately");

  info("Waiting " + (GRAB_DELAY + 1) + "ms");
  yield wait(GRAB_DELAY + 1);
  ok(el.isDragging, "isDragging true after GRAB_DELAY has passed");

  let dropCompleted = once(inspector.markup, "drop-completed");

  info("Simulating mouseUp on #test");
  el._onMouseUp({
    target: el.tagLine,
    pageX: rect.x,
    pageY: rect.y
  });

  yield dropCompleted;

  ok(!el.isDragging, "isDragging false after mouseUp");

  info("Simulating middle click on #test");
  el._onMouseDown({
    target: el.tagLine,
    button: 1,
    pageX: rect.x,
    pageY: rect.y,
    stopPropagation: function() {},
    preventDefault: function() {}
  });
  ok(!el.isDragging, "isDragging should not be set to true immediately");

  info("Waiting " + (GRAB_DELAY + 1) + "ms");
  yield wait(GRAB_DELAY + 1);
  ok(!el.isDragging, "isDragging never starts after middle click after mouseUp");
});
