/* vim: set ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Verify that if the animation's duration, iterations or delay change in
// content, then the widget reflects the changes.

add_task(function*() {
  yield addTab(TEST_URL_ROOT + "doc_simple_animation.html");

  let ui = yield openAnimationInspector();
  yield testDataUpdates(ui);

  info("Close the toolbox, reload the tab, and try again with the new UI");
  ui = yield closeAnimationInspectorAndRestartWithNewUI(true);
  yield testDataUpdates(ui, true);
});

function* testDataUpdates({panel, controller, inspector}, isNewUI=false) {
  info("Select the test node");
  yield selectNode(".animated", inspector);

  let animation = controller.animationPlayers[0];
  yield setStyle(animation, panel, "animationDuration", "5.5s", isNewUI);
  yield setStyle(animation, panel, "animationIterationCount", "300", isNewUI);
  yield setStyle(animation, panel, "animationDelay", "45s", isNewUI);

  if (isNewUI) {
    let animationsEl = panel.animationsTimelineComponent.animationsEl;
    let timeBlockEl = animationsEl.querySelector(".time-block");

    // 45s delay + (300 * 5.5)s duration
    let expectedTotalDuration = 1695 * 1000;
    let timeRatio = expectedTotalDuration / timeBlockEl.offsetWidth;

    // XXX: the nb and size of each iteration cannot be tested easily (displayed
    // using a linear-gradient background and capped at 2px wide). They should
    // be tested in bug 1173761.
    let delayWidth = parseFloat(timeBlockEl.querySelector(".delay").style.width);
    is(Math.round(delayWidth * timeRatio), 45 * 1000,
      "The timeline has the right delay");
  } else {
    let widget = panel.playerWidgets[0];
    is(widget.metaDataComponent.durationValue.textContent, "5.50s",
      "The widget shows the new duration");
    is(widget.metaDataComponent.iterationValue.textContent, "300",
      "The widget shows the new iteration count");
    is(widget.metaDataComponent.delayValue.textContent, "45s",
      "The widget shows the new delay");
  }
}

function* setStyle(animation, panel, name, value, isNewUI=false) {
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

  // If this is the playerWidget-based UI, wait for the auto-refresh event too
  // to make sure the UI has updated.
  if (!isNewUI) {
    yield once(animation, animation.AUTO_REFRESH_EVENT);
  }
}
