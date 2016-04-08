/* vim: set ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

requestLongerTimeout(2);

// Verify that if the animation's duration, iterations or delay change in
// content, then the widget reflects the changes.

add_task(function* () {
  yield addTab(URL_ROOT + "doc_simple_animation.html");
  let {panel, controller, inspector} = yield openAnimationInspector();

  info("Select the test node");
  yield selectNodeAndWaitForAnimations(".animated", inspector);

  let animation = controller.animationPlayers[0];
  yield setStyle(animation, panel, "animationDuration", "5.5s");
  yield setStyle(animation, panel, "animationIterationCount", "300");
  yield setStyle(animation, panel, "animationDelay", "45s");

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

function* setStyle(animation, panel, name, value) {
  info("Change the animation style via the content DOM. Setting " +
    name + " to " + value);

  let onAnimationChanged = once(animation, "changed");
  yield executeInContent("devtools:test:setStyle", {
    selector: ".animated",
    propertyName: name,
    propertyValue: value
  });
  yield onAnimationChanged;

  // Also wait for the target node previews to be loaded if the panel got
  // refreshed as a result of this animation mutation.
  yield waitForAllAnimationTargets(panel);
}
