/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test that whether animations detail could be displayed if there is selected animation.

add_task(async function () {
  await addTab(URL_ROOT + "doc_multi_timings.html");
  const { animationInspector, inspector, panel } = await openAnimationInspector();

  info("Checking animation detail visibility if animation was unselected");
  const detailEl = panel.querySelector("#animation-container .controlled");
  ok(detailEl, "The detail pane should be in the DOM");
  const win = panel.ownerGlobal;
  is(win.getComputedStyle(detailEl).display, "none", "detailEl should be unvisibled");

  info("Checking animation detail visibility if animation was selected by click");
  await clickOnAnimation(animationInspector, panel, 0);
  isnot(win.getComputedStyle(detailEl).display, "none", "detailEl should be visibled");

  info("Checking animation detail visibility when choose node which has animations");
  const htmlNode = await getNodeFront("html", inspector);
  await selectNodeAndWaitForAnimations(htmlNode, inspector);
  is(win.getComputedStyle(detailEl).display, "none",
     "detailEl should be unvisibled after choose html node");

  info("Checking animation detail visibility when choose node which has an animation");
  const animatedNode = await getNodeFront(".cssanimation-normal", inspector);
  await selectNodeAndWaitForAnimations(animatedNode, inspector);
  isnot(win.getComputedStyle(detailEl).display, "none",
        "detailEl should be visibled after choose .cssanimation-normal node");
});
