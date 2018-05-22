/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test whether pausing/resuming the each animations correctly.

add_task(async function() {
  await addTab(URL_ROOT + "doc_simple_animation.html");
  await removeAnimatedElementsExcept([".animated", ".compositor-all"]);
  const { animationInspector, inspector, panel } = await openAnimationInspector();
  const buttonEl = panel.querySelector(".pause-resume-button");

  info("Check '.compositor-all' animation is still running " +
       "after even pausing '.animated' animation");
  await selectNodeAndWaitForAnimations(".animated", inspector);
  await clickOnPauseResumeButton(animationInspector, panel);
  ok(buttonEl.classList.contains("paused"), "State of button should be paused");
  await selectNodeAndWaitForAnimations("body", inspector);
  assertStatus(animationInspector.state.animations, buttonEl,
               ["paused", "running"], false);

  info("Check both animations are paused after clicking pause/resume " +
       "while displaying both animations");
  await clickOnPauseResumeButton(animationInspector, panel);
  assertStatus(animationInspector.state.animations, buttonEl,
               ["paused", "paused"], true);

  info("Check '.animated' animation is still paused " +
       "after even resuming '.compositor-all' animation");
  await selectNodeAndWaitForAnimations(".compositor-all", inspector);
  await clickOnPauseResumeButton(animationInspector, panel);
  ok(!buttonEl.classList.contains("paused"), "State of button should be running");
  await selectNodeAndWaitForAnimations("body", inspector);
  assertStatus(animationInspector.state.animations, buttonEl,
               ["paused", "running"], false);
});

function assertStatus(animations, buttonEl,
                      expectedAnimationStates, shouldButtonPaused) {
  expectedAnimationStates.forEach((state, index) => {
    is(animations[index].state.playState, state, `Animation ${index} should be ${state}`);
  });

  is(buttonEl.classList.contains("paused"), shouldButtonPaused,
    "State of button is correct");
}
