/* vim: set ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Tests the behavior of animation-detail container.
// We test following cases.
// 1. Existance of animation-detail element.
// 2. Hidden at first if multiple animations were displayed.
// 3. Display after click on an animation.
// 4. Display from first time if displayed animation is only one.

requestLongerTimeout(5);

add_task(function* () {
  yield addTab(URL_ROOT + "doc_multiple_property_types.html");
  const { panel, inspector } = yield openAnimationInspector();
  const timelineComponent = panel.animationsTimelineComponent;
  const animationDetailEl =
    timelineComponent.rootWrapperEl.querySelector(".animation-detail");

  // 1. Existance of animation-detail element.
  ok(animationDetailEl, "The animation-detail element should exist");

  // 2. Hidden at first if multiple animations were displayed.
  const win = animationDetailEl.ownerDocument.defaultView;
  is(win.getComputedStyle(animationDetailEl).display, "none",
     "The animation-detail element should be hidden at first "
     + "if multiple animations were displayed");

  // 3. Display after click on an animation.
  yield clickOnAnimation(panel, 0);
  isnot(win.getComputedStyle(animationDetailEl).display, "none",
        "The animation-detail element should be displayed after clicked on an animation");

  // 4. Display from first time if displayed animation is only one.
  yield selectNodeAndWaitForAnimations("#target1", inspector);
  ok(animationDetailEl.querySelector(".property"),
     "The property in animation-detail element should be displayed");
});
