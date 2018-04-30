/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test for following selection feature related AnimationTarget component works:
// * select an animated node by clicking on target node
// * select selected node by clicking on target node

add_task(async function() {
  await addTab(URL_ROOT + "doc_simple_animation.html");
  await removeAnimatedElementsExcept([".multi", ".long"]);
  const { animationInspector, panel } = await openAnimationInspector();

  info("Check selecting an animated node by clicking on the target node");
  await clickOnTargetNode(animationInspector, panel, 0);
  is(panel.querySelectorAll(".animation-target").length, 2,
     "The length of animations should be 2, because .multi node has two animations");

  info("Check to avoid reselecting the already selected node");
  let isUpdated = false;
  const listener = () => {
    isUpdated = true;
  };
  animationInspector.on("animation-target-rendered", listener);
  const targetEl = panel.querySelectorAll(".animation-target .objectBox")[0];
  targetEl.scrollIntoView(false);
  EventUtils.synthesizeMouseAtCenter(targetEl, {}, targetEl.ownerGlobal);
  await wait(500);
  ok(!isUpdated, "Components should not have updated");
  animationInspector.off("animation-target-rendered", listener);
});
