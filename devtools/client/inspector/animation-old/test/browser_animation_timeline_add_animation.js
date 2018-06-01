/* vim: set ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test re-used animation element since we re-use existent animation element
// for the render performance.

add_task(async function() {
  await addTab(URL_ROOT + "doc_add_animation.html");
  const {panel, controller} = await openAnimationInspector();
  const timelineComponent = panel.animationsTimelineComponent;

  // Add new animation which has delay and endDelay.
  await startNewAnimation(controller, panel, "#target2");
  const previousAnimationEl =
    timelineComponent.animationsEl.querySelector(".animation:nth-child(2)");
  const previousSummaryGraphEl = previousAnimationEl.querySelector(".summary");
  const previousDelayEl = previousAnimationEl.querySelector(".delay");
  const previousEndDelayEl = previousAnimationEl.querySelector(".end-delay");
  const previousSummaryGraphWidth = previousSummaryGraphEl.viewBox.baseVal.width;
  const previousDelayBounds = previousDelayEl.getBoundingClientRect();
  const previousEndDelayBounds = previousEndDelayEl.getBoundingClientRect();

  // Add another animation.
  await startNewAnimation(controller, panel, "#target3");
  const currentAnimationEl =
    timelineComponent.animationsEl.querySelector(".animation:nth-child(2)");
  const currentSummaryGraphEl = currentAnimationEl.querySelector(".summary");
  const currentDelayEl = currentAnimationEl.querySelector(".delay");
  const currentEndDelayEl = currentAnimationEl.querySelector(".end-delay");
  const currentSummaryGraphWidth = currentSummaryGraphEl.viewBox.baseVal.width;
  const currentDelayBounds = currentDelayEl.getBoundingClientRect();
  const currentEndDelayBounds = currentEndDelayEl.getBoundingClientRect();

  is(previousAnimationEl, currentAnimationEl, ".animation element should be reused");
  is(previousSummaryGraphEl, currentSummaryGraphEl, ".summary element should be reused");
  is(previousDelayEl, currentDelayEl, ".delay element should be reused");
  is(previousEndDelayEl, currentEndDelayEl, ".end-delay element should be reused");

  ok(currentSummaryGraphWidth > previousSummaryGraphWidth,
     "Reused .summary element viewBox width should be longer");
  ok(currentDelayBounds.left < previousDelayBounds.left,
     "Reused .delay element should move to the left");
  ok(currentDelayBounds.width < previousDelayBounds.width,
     "Reused .delay element should be shorter");
  ok(currentEndDelayBounds.left < previousEndDelayBounds.left,
     "Reused .end-delay element should move to the left");
  ok(currentEndDelayBounds.width < previousEndDelayBounds.width,
     "Reused .end-delay element should be shorter");
});

async function startNewAnimation(controller, panel, selector) {
  info("Add a new animation to the page and check the time again");
  const onPlayerAdded = controller.once(controller.PLAYERS_UPDATED_EVENT);
  const onRendered = waitForAnimationTimelineRendering(panel);

  await executeInContent("devtools:test:setAttribute", {
    selector: selector,
    attributeName: "class",
    attributeValue: "animation"
  });

  await onPlayerAdded;
  await onRendered;
  await waitForAllAnimationTargets(panel);
}
