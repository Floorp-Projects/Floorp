/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test that when animations displayed in the timeline are running on the
// compositor, they get a special icon and information in the tooltip.

requestLongerTimeout(2);

add_task(async function () {
  await addTab(URL_ROOT + "doc_simple_animation.html");
  await removeAnimatedElementsExcept([
    ".compositor-all",
    ".compositor-notall",
    ".no-compositor",
  ]);
  const { animationInspector, inspector, panel } =
    await openAnimationInspector();

  info("Check animation whose all properties are running on compositor");
  const summaryGraphAllEl = await findSummaryGraph(".compositor-all", panel);
  ok(
    summaryGraphAllEl.classList.contains("compositor"),
    "The element has the compositor css class"
  );
  ok(
    hasTooltip(
      summaryGraphAllEl,
      ANIMATION_L10N.getStr("player.allPropertiesOnCompositorTooltip")
    ),
    "The element has the right tooltip content"
  );

  info("Check animation is not running on compositor");
  const summaryGraphNoEl = await findSummaryGraph(".no-compositor", panel);
  ok(
    !summaryGraphNoEl.classList.contains("compositor"),
    "The element does not have the compositor css class"
  );
  ok(
    !hasTooltip(
      summaryGraphNoEl,
      ANIMATION_L10N.getStr("player.allPropertiesOnCompositorTooltip")
    ),
    "The element does not have oncompositor tooltip content"
  );
  ok(
    !hasTooltip(
      summaryGraphNoEl,
      ANIMATION_L10N.getStr("player.somePropertiesOnCompositorTooltip")
    ),
    "The element does not have oncompositor tooltip content"
  );

  info(
    "Select a node has animation whose some properties are running on compositor"
  );
  await selectNode(".compositor-notall", inspector);
  const summaryGraphEl = await findSummaryGraph(".compositor-notall", panel);
  ok(
    summaryGraphEl.classList.contains("compositor"),
    "The element has the compositor css class"
  );
  ok(
    hasTooltip(
      summaryGraphEl,
      ANIMATION_L10N.getStr("player.somePropertiesOnCompositorTooltip")
    ),
    "The element has the right tooltip content"
  );

  info("Check compositor sign after pausing");
  clickOnPauseResumeButton(animationInspector, panel);
  await waitUntil(() => !summaryGraphEl.classList.contains("compositor"));
  ok(
    true,
    "The element should not have the compositor css class after pausing"
  );

  info("Check compositor sign after resuming");
  clickOnPauseResumeButton(animationInspector, panel);
  await waitUntil(() => summaryGraphEl.classList.contains("compositor"));
  ok(true, "The element should have the compositor css class after resuming");

  info("Check compositor sign after rewind");
  clickOnRewindButton(animationInspector, panel);
  await waitUntil(() => !summaryGraphEl.classList.contains("compositor"));
  ok(
    true,
    "The element should not have the compositor css class after rewinding"
  );
  clickOnPauseResumeButton(animationInspector, panel);
  await waitUntil(() => summaryGraphEl.classList.contains("compositor"));
  ok(true, "The element should have the compositor css class after resuming");

  info("Check compositor sign after setting the current time");
  clickOnCurrentTimeScrubberController(animationInspector, panel, 0.5);
  await waitUntil(() => !summaryGraphEl.classList.contains("compositor"));
  ok(
    true,
    "The element should not have the compositor css class " +
      "after setting the current time"
  );
  clickOnPauseResumeButton(animationInspector, panel);
  await waitUntil(() => summaryGraphEl.classList.contains("compositor"));
  ok(true, "The element should have the compositor css class after resuming");
});

async function findSummaryGraph(selector, panel) {
  const animationItemEl = await findAnimationItemByTargetSelector(
    panel,
    selector
  );
  return animationItemEl.querySelector(".animation-summary-graph");
}

function hasTooltip(summaryGraphEl, expected) {
  const tooltip = summaryGraphEl.getAttribute("title");
  return tooltip.includes(expected);
}
