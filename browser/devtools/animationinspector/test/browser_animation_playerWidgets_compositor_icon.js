/* vim: set ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test that player widgets show the right player meta-data for the
// isRunningOnCompositor property.

add_task(function*() {
  yield addTab(TEST_URL_ROOT + "doc_simple_animation.html");
  let {inspector, panel} = yield openAnimationInspector();

  info("Select the simple animated node");
  yield selectNode(".animated", inspector);

  let compositorEl = panel.playerWidgets[0]
                     .el.querySelector(".compositor-icon");

  ok(compositorEl, "The compositor-icon element exists");
  ok(isNodeVisible(compositorEl),
     "The compositor icon is visible, since the animation is running on " +
     "compositor thread");
});
