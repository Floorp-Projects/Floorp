/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test adjusting the created time with different playback rate of animation.

add_task(async function() {
  await addTab(URL_ROOT + "doc_custom_playback_rate.html");
  const { animationInspector, inspector, panel } = await openAnimationInspector();

  info("Pause the all animation and set current time to middle in order to check " +
       "the adjusting time");
  await clickOnPauseResumeButton(animationInspector, panel);
  await clickOnCurrentTimeScrubberController(animationInspector, panel, 0.5);

  info("Check the created times of all animation are same");
  checkAdjustingTheTime(animationInspector.state.animations[0].state,
                        animationInspector.state.animations[1].state);

  info("Change the playback rate to x10 after selecting '.div2'");
  await selectNodeAndWaitForAnimations(".div2", inspector);
  await clickOnPlaybackRateSelector(animationInspector, panel, 10);

  info("Check each adjusted result of animations after selecting 'body' again");
  await selectNodeAndWaitForAnimations("body", inspector);

  checkAdjustingTheTime(animationInspector.state.animations[0].state,
                        animationInspector.state.animations[1].state);
  is(animationInspector.state.animations[0].state.currentTime, 50000,
     "The current time of '.div1' animation is 50%");
  is(animationInspector.state.animations[1].state.currentTime, 50000,
     "The current time of '.div2' animation is 50%");
});
