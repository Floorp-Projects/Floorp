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
// 5. Close the animation-detail element by clicking on close button.
// 6. Stay selected animation even if refresh all UI.
// 7. Close the animation-detail element again and click selected animation again.

requestLongerTimeout(5);

add_task(function* () {
  yield addTab(URL_ROOT + "doc_multiple_property_types.html");
  const { panel, inspector } = yield openAnimationInspector();
  const timelineComponent = panel.animationsTimelineComponent;
  const animationDetailEl =
    timelineComponent.rootWrapperEl.querySelector(".animation-detail");
  const splitboxControlledEl =
    timelineComponent.rootWrapperEl.querySelector(".controlled");

  // 1. Existance of animation-detail element.
  ok(animationDetailEl, "The animation-detail element should exist");

  // 2. Hidden at first if multiple animations were displayed.
  const win = timelineComponent.rootWrapperEl.ownerGlobal;
  is(win.getComputedStyle(splitboxControlledEl).display, "none",
     "The animation-detail element should be hidden at first "
     + "if multiple animations were displayed");

  // 3. Display after click on an animation.
  yield clickOnAnimation(panel, 0);
  isnot(win.getComputedStyle(splitboxControlledEl).display, "none",
        "The animation-detail element should be displayed after clicked on an animation");

  // 4. Display from first time if displayed animation is only one.
  yield selectNodeAndWaitForAnimations("#target1", inspector);
  ok(animationDetailEl.querySelector(".property"),
     "The property in animation-detail element should be displayed");

  // 5. Close the animation-detail element by clicking on close button.
  const previousHeight = animationDetailEl.offsetHeight;
  yield clickCloseButtonForDetailPanel(timelineComponent, animationDetailEl);
  is(win.getComputedStyle(splitboxControlledEl).display, "none",
     "animation-detail element should not display");

  // Select another animation.
  yield selectNodeAndWaitForAnimations("#target2", inspector);
  isnot(win.getComputedStyle(splitboxControlledEl).display, "none",
        "animation-detail element should display");
  is(animationDetailEl.offsetHeight, previousHeight,
     "The height of animation-detail should keep the height");

  // 6. Stay selected animation even if refresh all UI.
  yield selectNodeAndWaitForAnimations("#target1", inspector);
  yield clickTimelineRewindButton(panel);
  ok(animationDetailEl.querySelector(".property"),
     "The property in animation-detail element should stay as is");

  // 7. Close the animation-detail element again and click selected animation again.
  yield clickCloseButtonForDetailPanel(timelineComponent, animationDetailEl);
  yield clickOnAnimation(panel, 0);
  isnot(win.getComputedStyle(splitboxControlledEl).display, "none",
     "animation-detail element should display again");
});

/**
 * Click close button for animation-detail panel.
 *
 * @param {AnimationTimeline} AnimationTimeline component
 * @param {DOMNode} animation-detail element
 * @return {Promise} which wait for close the detail pane
 */
function* clickCloseButtonForDetailPanel(timeline, element) {
  const button = element.querySelector(".animation-detail-header button");
  const onclosed = timeline.once("animation-detail-closed");
  EventUtils.sendMouseEvent({type: "click"}, button, element.ownerDocument.defaultView);
  return yield onclosed;
}
