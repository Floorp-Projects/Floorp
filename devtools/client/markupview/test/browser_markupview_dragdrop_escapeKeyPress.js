/* vim: set ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test whether ESCAPE keypress cancels dragging of an element

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

  info("Waiting for a little bit more than the markup-view grab delay");
  yield wait(GRAB_DELAY + 1);
  ok(el.isDragging, "isDragging true after mouseDown");

  info("Simulating ESCAPE keypress");
  EventUtils.sendKey("escape", inspector.panelWin);
  is(el.isDragging, false, "isDragging false after ESCAPE keypress");
});
