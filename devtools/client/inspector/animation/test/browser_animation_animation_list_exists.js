/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test that whether animations ui could be displayed

add_task(async function () {
  await addTab(URL_ROOT + "doc_simple_animation.html");

  const { animationInspector, inspector, panel } = await openAnimationInspector();

  info("Checking animation list and items existence");
  ok(panel.querySelector(".animation-list"),
     "The animation-list is in the DOM");
  is(panel.querySelectorAll(".animation-list .animation-item").length,
     animationInspector.animations.length,
     "The number of animations displayed matches the number of animations");

  info("Checking the background color for the animation list items");
  const animationItemEls = panel.querySelectorAll(".animation-list .animation-item");
  const evenColor =
    panel.ownerGlobal.getComputedStyle(animationItemEls[0]).backgroundColor;
  const oddColor =
    panel.ownerGlobal.getComputedStyle(animationItemEls[1]).backgroundColor;
  isnot(evenColor, oddColor,
        "Background color of an even animation should be different from odd");

  info("Checking list and items existence after select a element which has an animation");
  const animatedNode = await getNodeFront(".animated", inspector);
  await selectNodeAndWaitForAnimations(animatedNode, inspector);
  is(panel.querySelectorAll(".animation-list .animation-item").length, 1,
     "The number of animations displayed should be 1 for .animated element");

  // TODO: We need to add following tests after implement since this test has same role
  // of animationinspector/test/browser_animation_timeline_ui.js
  // * name label in animation element existance.
  // * summary graph in animation element existance.
});
