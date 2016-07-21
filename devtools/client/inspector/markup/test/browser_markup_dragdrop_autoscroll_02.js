/* vim: set ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test that dragging a node near the top or bottom edge of the markup-view
// auto-scrolls the view on a small toolbox.

const TEST_URL = URL_ROOT + "doc_markup_dragdrop_autoscroll_02.html";

add_task(function* () {
  // Set the toolbox to very small in size.
  yield pushPref("devtools.toolbox.footer.height", 150);

  let {inspector} = yield openInspectorForURL(TEST_URL);
  let markup = inspector.markup;
  let viewHeight = markup.doc.documentElement.clientHeight;

  info("Pretend the markup-view is dragging");
  markup.isDragging = true;

  info("Simulate a mousemove on the view, at the bottom, and expect scrolling");
  let onScrolled = waitForScrollStop(markup.doc);

  markup._onMouseMove({
    preventDefault: () => {},
    target: markup.doc.body,
    pageY: viewHeight + markup.doc.defaultView.scrollY
  });

  let bottomScrollPos = yield onScrolled;
  ok(bottomScrollPos > 0, "The view was scrolled down");
  info("Simulate a mousemove at the top and expect more scrolling");
  onScrolled = waitForScrollStop(markup.doc);

  markup._onMouseMove({
    preventDefault: () => {},
    target: markup.doc.body,
    pageY: markup.doc.defaultView.scrollY
  });

  let topScrollPos = yield onScrolled;
  ok(topScrollPos < bottomScrollPos, "The view was scrolled up");
  is(topScrollPos, 0, "The view was scrolled up to the top");

  info("Simulate a mouseup to stop dragging");
  markup._onMouseUp();
});
