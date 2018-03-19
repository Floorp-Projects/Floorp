/* vim: set ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

requestLongerTimeout(2);

// Test that when animations displayed in the timeline are running on the
// compositor, they get a special icon and information in the tooltip.

add_task(async function() {
  await addTab(URL_ROOT + "doc_simple_animation.html");
  let {inspector, panel} = await openAnimationInspector();
  let timeline = panel.animationsTimelineComponent;

  info("Select a test node we know has an animation running on the compositor");
  await selectNodeAndWaitForAnimations(".compositor-all", inspector);

  let animationEl = timeline.animationsEl.querySelector(".animation");
  ok(animationEl.classList.contains("fast-track"),
     "The animation element has the fast-track css class");
  ok(hasTooltip(animationEl,
                ANIMATION_L10N.getStr("player.allPropertiesOnCompositorTooltip")),
     "The animation element has the right tooltip content");

  info("Select a node we know doesn't have an animation on the compositor");
  await selectNodeAndWaitForAnimations(".no-compositor", inspector);

  animationEl = timeline.animationsEl.querySelector(".animation");
  ok(!animationEl.classList.contains("fast-track"),
     "The animation element does not have the fast-track css class");
  ok(!hasTooltip(animationEl,
                 ANIMATION_L10N.getStr("player.allPropertiesOnCompositorTooltip")),
     "The animation element does not have oncompositor tooltip content");
  ok(!hasTooltip(animationEl,
                 ANIMATION_L10N.getStr("player.somePropertiesOnCompositorTooltip")),
     "The animation element does not have oncompositor tooltip content");

  info("Select a node we know has animation on the compositor and not on the" +
       " compositor");
  await selectNodeAndWaitForAnimations(".compositor-notall", inspector);

  animationEl = timeline.animationsEl.querySelector(".animation");
  ok(animationEl.classList.contains("fast-track"),
     "The animation element has the fast-track css class");
  ok(hasTooltip(animationEl,
                ANIMATION_L10N.getStr("player.somePropertiesOnCompositorTooltip")),
     "The animation element has the right tooltip content");
});

function hasTooltip(animationEl, expected) {
  let el = animationEl.querySelector(".name");
  let tooltip = el.getAttribute("title");

  return tooltip.includes(expected);
}
