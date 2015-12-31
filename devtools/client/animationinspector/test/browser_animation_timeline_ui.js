/* vim: set ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

requestLongerTimeout(2);

// Check that the timeline contains the right elements.

add_task(function*() {
  yield addTab(TEST_URL_ROOT + "doc_simple_animation.html");
  let {panel} = yield openAnimationInspector();

  let timeline = panel.animationsTimelineComponent;
  let el = timeline.rootWrapperEl;

  ok(el.querySelector(".time-header"),
     "The header element is in the DOM of the timeline");
  ok(el.querySelectorAll(".time-header .time-tick").length,
     "The header has some time graduations");

  ok(el.querySelector(".animations"),
     "The animations container is in the DOM of the timeline");
  is(el.querySelectorAll(".animations .animation").length,
     timeline.animations.length,
     "The number of animations displayed matches the number of animations");

  for (let i = 0; i < timeline.animations.length; i++) {
    let animation = timeline.animations[i];
    let animationEl = el.querySelectorAll(".animations .animation")[i];

    ok(animationEl.querySelector(".target"),
       "The animated node target element is in the DOM");
    ok(animationEl.querySelector(".time-block"),
       "The timeline element is in the DOM");
    is(animationEl.querySelector(".name").textContent,
       animation.state.name,
       "The name on the timeline is correct");
    ok(animationEl.querySelector(".iterations"),
       "The timeline has iterations displayed");
  }
});
