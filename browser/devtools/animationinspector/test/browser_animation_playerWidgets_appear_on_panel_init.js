/* vim: set ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test that player widgets are displayed right when the animation panel is
// initialized, if the selected node (<body> by default) is animated.

add_task(function*() {
  yield addTab(TEST_URL_ROOT + "doc_body_animation.html");

  let {panel} = yield openAnimationInspector();
  is(panel.animationsTimelineComponent.animations.length, 1,
    "One animation is handled by the timeline after init");
  is(panel.animationsTimelineComponent.animationsEl.childNodes.length, 1,
    "One animation is displayed after init");
});
