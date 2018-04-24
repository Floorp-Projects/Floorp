/* vim: set ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

requestLongerTimeout(2);

// Check that the timeline contains the right elements.

add_task(async function() {
  await addTab(URL_ROOT + "doc_simple_animation.html");
  let {panel} = await openAnimationInspector();

  let timeline = panel.animationsTimelineComponent;
  let el = timeline.rootWrapperEl;

  ok(el.querySelector(".time-header"),
     "The header element is in the DOM of the timeline");
  ok(el.querySelectorAll(".time-header .header-item").length,
     "The header has some time graduations");

  ok(el.querySelector(".animations"),
     "The animations container is in the DOM of the timeline");
  is(el.querySelectorAll(".animations .animation").length,
     timeline.animations.length,
     "The number of animations displayed matches the number of animations");

  const animationEls = el.querySelectorAll(".animations .animation");
  const evenColor =
    el.ownerDocument.defaultView.getComputedStyle(animationEls[0]).backgroundColor;
  const oddColor =
    el.ownerDocument.defaultView.getComputedStyle(animationEls[1]).backgroundColor;
  isnot(evenColor, oddColor,
        "Background color of an even animation should be different from odd");
  for (let i = 0; i < timeline.animations.length; i++) {
    let animation = timeline.animations[i];
    let animationEl = animationEls[i];

    ok(animationEl.querySelector(".target"),
       "The animated node target element is in the DOM");
    ok(animationEl.querySelector(".time-block"),
       "The timeline element is in the DOM");
    is(animationEl.querySelector(".name").textContent,
       animation.state.name,
       "The name on the timeline is correct");
    is(animationEl.querySelectorAll("svg g").length, 1,
       "The g element should be one since this doc's all animation has only one shape");
    ok(animationEl.querySelector("svg g path"),
       "The timeline has svg and path element as summary graph");

    const expectedBackgroundColor = i % 2 === 0 ? evenColor : oddColor;
    const backgroundColor =
      el.ownerDocument.defaultView.getComputedStyle(animationEl).backgroundColor;
    is(backgroundColor, expectedBackgroundColor,
       "The background-color shoud be changed to alternate");
  }
});
