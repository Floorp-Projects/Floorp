/* vim: set ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

requestLongerTimeout(2);

// Test that the panel shows an animation player when an animated node is
// selected.

add_task(function* () {
  yield addTab(URL_ROOT + "doc_simple_animation.html");
  let {inspector, panel} = yield openAnimationInspector();

  info("Select node .animated and check that the panel is not empty");
  let node = yield getNodeFront(".animated", inspector);
  yield selectNodeAndWaitForAnimations(node, inspector);

  assertAnimationsDisplayed(panel, 1);
});
