/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test that when animations displayed in the timeline are running on the
// compositor, they get a special icon and information in the tooltip.

add_task(async function () {
  await addTab(URL_ROOT + "doc_simple_animation.html");

  const { inspector, panel } = await openAnimationInspector();

  info("Select a test node we know has an animation running on the compositor");
  await selectNodeAndWaitForAnimations(".compositor-all", inspector);

  const summaryGraphEl = panel.querySelector(".animation-summary-graph");
  ok(summaryGraphEl.classList.contains("compositor"),
     "The element has the compositor css class");
  ok(hasTooltip(summaryGraphEl,
                ANIMATION_L10N.getStr("player.allPropertiesOnCompositorTooltip")),
     "The element has the right tooltip content");

  info("Select a node we know doesn't have an animation on the compositor");
  await selectNodeAndWaitForAnimations(".no-compositor", inspector);

  ok(!summaryGraphEl.classList.contains("compositor"),
     "The element does not have the compositor css class");
  ok(!hasTooltip(summaryGraphEl,
                 ANIMATION_L10N.getStr("player.allPropertiesOnCompositorTooltip")),
     "The element does not have oncompositor tooltip content");
  ok(!hasTooltip(summaryGraphEl,
                 ANIMATION_L10N.getStr("player.somePropertiesOnCompositorTooltip")),
     "The element does not have oncompositor tooltip content");

  info("Select a node we know has animation on the compositor and not on the compositor");
  await selectNodeAndWaitForAnimations(".compositor-notall", inspector);

  ok(summaryGraphEl.classList.contains("compositor"),
     "The element has the compositor css class");
  ok(hasTooltip(summaryGraphEl,
                ANIMATION_L10N.getStr("player.somePropertiesOnCompositorTooltip")),
     "The element has the right tooltip content");
});

function hasTooltip(summaryGraphEl, expected) {
  const tooltip = summaryGraphEl.getAttribute("title");
  return tooltip.includes(expected);
}
