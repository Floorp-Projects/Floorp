/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test the Computed Timing Path component for different time scales.

add_task(async function () {
  await addTab(URL_ROOT + "doc_simple_animation.html");
  await removeAnimatedElementsExcept([".animated", ".end-delay"]);
  const { animationInspector, inspector, panel } =
    await openAnimationInspector();

  info("Checking the path for different time scale");
  let onDetailRendered = animationInspector.once(
    "animation-keyframes-rendered"
  );
  await selectNode(".animated", inspector);
  await onDetailRendered;
  const itemA = await findAnimationItemByTargetSelector(panel, ".animated");
  const pathStringA = itemA
    .querySelector(".animation-iteration-path")
    .getAttribute("d");

  info("Select animation which has different time scale from no-compositor");
  onDetailRendered = animationInspector.once("animation-keyframes-rendered");
  await selectNode(".end-delay", inspector);
  await onDetailRendered;

  info("Select no-compositor again");
  onDetailRendered = animationInspector.once("animation-keyframes-rendered");
  await selectNode(".animated", inspector);
  await onDetailRendered;
  const itemB = await findAnimationItemByTargetSelector(panel, ".animated");
  const pathStringB = itemB
    .querySelector(".animation-iteration-path")
    .getAttribute("d");
  is(
    pathStringA,
    pathStringB,
    "Path string should be same even change the time scale"
  );
});
