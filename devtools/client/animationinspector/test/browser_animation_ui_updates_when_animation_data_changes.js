/* vim: set ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

requestLongerTimeout(2);

// Verify that if the animation's duration, iterations or delay change in
// content, then the widget reflects the changes.

add_task(async function() {
  await addTab(URL_ROOT + "doc_simple_animation.html");
  let {panel, controller, inspector} = await openAnimationInspector();

  info("Select the test node");
  await selectNodeAndWaitForAnimations(".animated", inspector);

  let animation = controller.animationPlayers[0];
  await setStyle(animation, panel, "animationDuration", "5.5s", ".animated");
  await setStyle(animation, panel, "animationIterationCount", "300", ".animated");
  await setStyle(animation, panel, "animationDelay", "45s", ".animated");

  let animationsEl = panel.animationsTimelineComponent.animationsEl;
  let timeBlockEl = animationsEl.querySelector(".time-block");

  // 45s delay + (300 * 5.5)s duration
  let expectedTotalDuration = 1695 * 1000;

  // XXX: the nb and size of each iteration cannot be tested easily (displayed
  // using a linear-gradient background and capped at 2px wide). They should
  // be tested in bug 1173761.
  let delayWidth = parseFloat(timeBlockEl.querySelector(".delay").style.width);
  is(Math.round(delayWidth * expectedTotalDuration / 100), 45 * 1000,
    "The timeline has the right delay");
});
