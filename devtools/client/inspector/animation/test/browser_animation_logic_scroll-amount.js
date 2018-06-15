/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test whether the scroll amount of animation and animated property re-calculate after
// changing selected node.

add_task(async function() {
  await addTab(URL_ROOT + "doc_simple_animation.html");
  await removeAnimatedElementsExcept(
    [".animated", ".multi", ".longhand", ".negative-delay"]);
  const { animationInspector, inspector, panel } = await openAnimationInspector();

  info("Set the scroll amount of animation and animated property to the bottom");
  await clickOnAnimationByTargetSelector(animationInspector, panel, ".longhand");
  const bottomAnimationEl =
    panel.querySelector(".animation-item:last-child");
  const bottomAnimatedPropertyEl =
    panel.querySelector(".animated-property-item:last-child");
  bottomAnimationEl.scrollIntoView(false);
  bottomAnimatedPropertyEl.scrollIntoView(false);

  info("Hold the scroll amount");
  const animationInspectionPanel =
    bottomAnimationEl.closest(".progress-inspection-panel");
  const animatedPropertyInspectionPanel =
    bottomAnimatedPropertyEl.closest(".progress-inspection-panel");
  const initialScrollTopOfAnimation = animationInspectionPanel.scrollTop;
  const initialScrollTopOfAnimatedProperty = animatedPropertyInspectionPanel.scrollTop;

  info("Check whether the scroll amount re-calculate after changing the count of items");
  await selectNodeAndWaitForAnimations(".negative-delay", inspector);
  ok(initialScrollTopOfAnimation > animationInspectionPanel.scrollTop,
    "Scroll amount for animation list should be less than previous state");
  ok(initialScrollTopOfAnimatedProperty > animatedPropertyInspectionPanel.scrollTop,
    "Scroll amount for animated property list should be less than previous state");
});
