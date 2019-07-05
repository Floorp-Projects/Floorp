/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test the Computed Timing Path component for different time scales.

add_task(async function() {
  await addTab(URL_ROOT + "doc_simple_animation.html");
  await removeAnimatedElementsExcept([".animated", ".end-delay"]);
  const { inspector, panel } = await openAnimationInspector();

  info("Checking the path for different time scale");
  await selectNodeAndWaitForAnimations(".animated", inspector);
  const pathStringA = panel
    .querySelector(".animation-iteration-path")
    .getAttribute("d");

  info("Select animation which has different time scale from no-compositor");
  await selectNodeAndWaitForAnimations(".end-delay", inspector);

  info("Select no-compositor again");
  await selectNodeAndWaitForAnimations(".animated", inspector);
  const pathStringB = panel
    .querySelector(".animation-iteration-path")
    .getAttribute("d");
  is(
    pathStringA,
    pathStringB,
    "Path string should be same even change the time scale"
  );
});
