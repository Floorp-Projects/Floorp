/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test whether the scroll amount of animation and animated property re-calculate after
// changing selected node.

add_task(async function () {
  await addTab(URL_ROOT + "doc_simple_animation.html");
  await removeAnimatedElementsExcept([
    ".animated",
    ".multi",
    ".longhand",
    ".negative-delay",
  ]);
  const { animationInspector, inspector, panel } =
    await openAnimationInspector();

  info(
    "Set the scroll amount of animation and animated property to the bottom"
  );
  const onDetailRendered = animationInspector.once(
    "animation-keyframes-rendered"
  );
  await clickOnAnimationByTargetSelector(
    animationInspector,
    panel,
    ".longhand"
  );
  await onDetailRendered;

  await waitUntil(() => panel.querySelectorAll(".animation-item").length === 5);
  const bottomAnimationEl = await findAnimationItemByIndex(panel, 4);
  const bottomAnimatedPropertyEl = panel.querySelector(
    ".animated-property-item:last-child"
  );
  bottomAnimationEl.scrollIntoView(false);
  bottomAnimatedPropertyEl.scrollIntoView(false);

  info("Hold the scroll amount");
  const animationInspectionPanel = bottomAnimationEl.closest(
    ".progress-inspection-panel"
  );
  const animatedPropertyInspectionPanel = bottomAnimatedPropertyEl.closest(
    ".progress-inspection-panel"
  );
  const initialScrollTopOfAnimation = animationInspectionPanel.scrollTop;
  const initialScrollTopOfAnimatedProperty =
    animatedPropertyInspectionPanel.scrollTop;

  info(
    "Check whether the scroll amount re-calculate after changing the count of items"
  );
  await selectNode(".negative-delay", inspector);
  await waitUntil(
    () =>
      initialScrollTopOfAnimation > animationInspectionPanel.scrollTop &&
      initialScrollTopOfAnimatedProperty >
        animatedPropertyInspectionPanel.scrollTop
  );
  ok(
    true,
    "Scroll amount for animation list should be less than previous state"
  );
  ok(
    true,
    "Scroll amount for animated property list should be less than previous state"
  );
});
