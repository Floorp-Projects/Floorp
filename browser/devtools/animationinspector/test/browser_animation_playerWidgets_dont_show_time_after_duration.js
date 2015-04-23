/* vim: set ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test that after the animation has ended, the current time label and timeline
// slider don't show values bigger than the animation duration (which would
// happen if the local requestAnimationFrame loop didn't stop correctly).

let L10N = new ViewHelpers.L10N();

add_task(function*() {
  yield addTab(TEST_URL_ROOT + "doc_simple_animation.html");
  let {inspector, panel} = yield openAnimationInspector();

  info("Select the test node");
  yield selectNode(".still", inspector);

  info("Start an animation on the test node and wait for the widget to appear");
  let onUiUpdated = panel.once(panel.UI_UPDATED_EVENT);
  yield executeInContent("devtools:test:setAttribute", {
    selector: ".still",
    attributeName: "class",
    attributeValue: "ball still short"
  });
  yield onUiUpdated;

  info("Wait until the animation ends");
  let widget = panel.playerWidgets[0];
  let front = widget.player;

  yield waitForPlayState(front, "finished");

  is(widget.currentTimeEl.value, front.state.duration,
    "The timeline slider has the right value");
  is(widget.timeDisplayEl.textContent,
    L10N.numberWithDecimals(front.state.duration / 1000, 2) + "s",
    "The timeline slider has the right value");
});
