/* vim: set ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test that dragging a node near the top or bottom edge of the markup-view
// auto-scrolls the view.

const TEST_URL = TEST_URL_ROOT + "doc_markup_dragdrop_autoscroll.html";

add_task(function*() {
  let {inspector} = yield addTab(TEST_URL).then(openInspector);
  let markup = inspector.markup;
  let viewHeight = markup.doc.documentElement.clientHeight;

  info("Pretend the markup-view is dragging");
  markup.isDragging = true;

  info("Simulate a mousemove on the view, at the bottom, and expect scrolling");
  let onScrolled = waitForScrollStop(markup);

  markup._onMouseMove({
    preventDefault: () => {},
    target: markup.doc.body,
    pageY: viewHeight
  });

  let bottomScrollPos = yield onScrolled;
  ok(bottomScrollPos > 0, "The view was scrolled down");

  info("Simulate a mousemove at the top and expect more scrolling");
  onScrolled = waitForScrollStop(markup);

  markup._onMouseMove({
    preventDefault: () => {},
    target: markup.doc.body,
    pageY: 0
  });

  let topScrollPos = yield onScrolled;
  ok(topScrollPos < bottomScrollPos, "The view was scrolled up");
  is(topScrollPos, 0, "The view was scrolled up to the top");

  info("Simulate a mouseup to stop dragging");
  markup._onMouseUp();
});

/**
 * Waits until the element has not scrolled for 30 consecutive frames.
 */
function* waitForScrollStop(markup) {
  let el = markup.doc.documentElement;
  let win = markup.doc.defaultView;
  let lastScrollTop = el.scrollTop;
  let stopFrameCount = 0;
  while (stopFrameCount < 30) {
    // Wait for a frame.
    yield new Promise(resolve => win.requestAnimationFrame(resolve));

    // Check if the element has scrolled.
    if (lastScrollTop == el.scrollTop) {
      // No scrolling since the last frame.
      stopFrameCount++;
    } else {
      // The element has scrolled. Reset the frame counter.
      stopFrameCount = 0;
      lastScrollTop = el.scrollTop;
    }
  }

  return lastScrollTop;
}
