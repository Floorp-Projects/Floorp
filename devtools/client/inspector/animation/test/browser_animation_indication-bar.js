/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test whether the indication bar of both scrubber and progress bar indicates correct
// progress after resizing animation inspector.

add_task(async function() {
  await pushPref("devtools.inspector.three-pane-enabled", false);
  await addTab(URL_ROOT + "doc_simple_animation.html");
  await removeAnimatedElementsExcept([".animated"]);
  const { animationInspector, inspector, panel } = await openAnimationInspector();

  info("Checking timeline tick item elements after enlarge sidebar width");
  await clickOnCurrentTimeScrubberController(animationInspector, panel, 0.5);
  await setSidebarWidth("100%", inspector);
  assertPosition(".current-time-scrubber", panel, 0.5);
  assertPosition(".keyframes-progress-bar", panel, 0.5);
});

/**
 * Assert indication bar position.
 *
 * @param {String} indicationBarSelector
 * @param {Element} panel
 * @param {Number} expectedPositionRate
 */
function assertPosition(indicationBarSelector, panel, expectedPositionRate) {
  const barEl = panel.querySelector(indicationBarSelector);
  const parentEl = barEl.parentNode;
  const rectBar = barEl.getBoundingClientRect();
  const rectParent = parentEl.getBoundingClientRect();
  const barX = rectBar.x + rectBar.width * 0.5 - rectParent.x;
  const expectedPosition = rectParent.width * expectedPositionRate;
  ok(expectedPosition - 1 <= barX && barX <= expectedPosition + 1,
    `Indication bar position should be approximately ${ expectedPosition }`);
}
