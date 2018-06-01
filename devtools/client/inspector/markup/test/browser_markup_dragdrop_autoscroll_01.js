/* vim: set ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test that dragging a node near the top or bottom edge of the markup-view
// auto-scrolls the view on a large toolbox.

const TEST_URL = URL_ROOT + "doc_markup_dragdrop_autoscroll_01.html";

add_task(async function() {
  // Set the toolbox as large as it would get. The toolbox automatically shrinks
  // to not overflow to window.
  await pushPref("devtools.toolbox.footer.height", 10000);

  const {inspector} = await openInspectorForURL(TEST_URL);
  const markup = inspector.markup;
  const viewHeight = markup.doc.documentElement.clientHeight;

  info("Pretend the markup-view is dragging");
  markup.isDragging = true;

  info("Simulate a mousemove on the view, at the bottom, and expect scrolling");

  markup._onMouseMove({
    preventDefault: () => {},
    target: markup.doc.body,
    pageY: viewHeight + markup.doc.defaultView.scrollY
  });

  const bottomScrollPos = await waitForScrollStop(markup.doc);
  ok(bottomScrollPos > 0, "The view was scrolled down");

  info("Simulate a mousemove at the top and expect more scrolling");

  markup._onMouseMove({
    preventDefault: () => {},
    target: markup.doc.body,
    pageY: markup.doc.defaultView.scrollY
  });

  const topScrollPos = await waitForScrollStop(markup.doc);
  ok(topScrollPos < bottomScrollPos, "The view was scrolled up");
  is(topScrollPos, 0, "The view was scrolled up to the top");

  info("Simulate a mouseup to stop dragging");
  markup._onMouseUp();
});
