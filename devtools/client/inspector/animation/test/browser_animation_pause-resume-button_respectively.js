/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test whether pausing/resuming the each animations correctly.

add_task(async function () {
  await addTab(URL_ROOT + "doc_simple_animation.html");
  await removeAnimatedElementsExcept([".animated", ".compositor-all"]);
  const { animationInspector, inspector, panel } =
    await openAnimationInspector();
  const buttonEl = panel.querySelector(".pause-resume-button");

  info(
    "Check '.compositor-all' animation is still running " +
      "after even pausing '.animated' animation"
  );
  await selectNode(".animated", inspector);
  clickOnPauseResumeButton(animationInspector, panel);
  await waitUntilAnimationsPlayState(animationInspector, "paused");
  ok(buttonEl.classList.contains("paused"), "State of button should be paused");
  await selectNode("body", inspector);
  await assertStatus(
    animationInspector.state.animations,
    buttonEl,
    ["paused", "running"],
    false
  );

  info(
    "Check both animations are paused after clicking pause/resume " +
      "while displaying both animations"
  );
  clickOnPauseResumeButton(animationInspector, panel);
  await assertStatus(
    animationInspector.state.animations,
    buttonEl,
    ["paused", "paused"],
    true
  );

  info(
    "Check '.animated' animation is still paused " +
      "after even resuming '.compositor-all' animation"
  );
  await selectNode(".compositor-all", inspector);
  clickOnPauseResumeButton(animationInspector, panel);
  await waitUntil(() =>
    animationInspector.state.animations.some(
      a => a.state.playState === "running"
    )
  );
  ok(
    !buttonEl.classList.contains("paused"),
    "State of button should be running"
  );
  await selectNode("body", inspector);
  await assertStatus(
    animationInspector.state.animations,
    buttonEl,
    ["paused", "running"],
    false
  );
});

async function assertStatus(
  animations,
  buttonEl,
  expectedAnimationStates,
  shouldButtonPaused
) {
  await waitUntil(() => {
    for (let i = 0; i < expectedAnimationStates.length; i++) {
      const animation = animations[i];
      const state = expectedAnimationStates[i];
      if (animation.state.playState !== state) {
        return false;
      }
    }
    return true;
  });
  expectedAnimationStates.forEach((state, index) => {
    is(
      animations[index].state.playState,
      state,
      `Animation ${index} should be ${state}`
    );
  });

  is(
    buttonEl.classList.contains("paused"),
    shouldButtonPaused,
    "State of button is correct"
  );
}
