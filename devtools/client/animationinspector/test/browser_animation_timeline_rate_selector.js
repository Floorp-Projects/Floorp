/* vim: set ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

requestLongerTimeout(2);

// Check that the timeline toolbar contains a playback rate selector UI and that
// it can be used to change the playback rate of animations in the timeline.
// Also check that it displays the rate of the current animations in case they
// all have the same rate, or that it displays the empty value in case they
// have mixed rates.

add_task(async function() {
  await addTab(URL_ROOT + "doc_simple_animation.html");

  let {panel, controller, inspector, toolbox} = await openAnimationInspector();

  // In this test, we disable the highlighter on purpose because of the way
  // events are simulated to select an option in the playbackRate <select>.
  // Indeed, this may cause mousemove events to be triggered on the nodes that
  // are underneath the <select>, and these are AnimationTargetNode instances.
  // Simulating mouse events on them will cause the highlighter to emit requests
  // and this might cause the test to fail if they happen after it has ended.
  disableHighlighter(toolbox);

  let select = panel.rateSelectorEl.firstChild;

  ok(select, "The rate selector exists");

  info("Change all of the current animations' rates to 0.5");
  await changeTimelinePlaybackRate(panel, .5);
  checkAllAnimationsRatesChanged(controller, select, .5);

  info("Select just one animated node and change its rate only");
  await selectNodeAndWaitForAnimations(".animated", inspector);

  await changeTimelinePlaybackRate(panel, 2);
  checkAllAnimationsRatesChanged(controller, select, 2);

  info("Select the <body> again, it should now have mixed-rates animations");
  await selectNodeAndWaitForAnimations("body", inspector);

  is(select.value, "", "The selected rate is empty");

  info("Change the rate for these mixed-rate animations");
  await changeTimelinePlaybackRate(panel, 1);
  checkAllAnimationsRatesChanged(controller, select, 1);
});

function checkAllAnimationsRatesChanged({animationPlayers}, select, rate) {
  ok(animationPlayers.every(({state}) => state.playbackRate === rate),
     "All animations' rates have been set to " + rate);
  is(select.value, rate, "The right value is displayed in the select");
}
