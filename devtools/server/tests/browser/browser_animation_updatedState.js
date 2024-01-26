/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */
/* eslint-disable mozilla/no-arbitrary-setTimeout */

"use strict";

// Check the animation player's updated state

add_task(async function () {
  const { target, walker, animations } = await initAnimationsFrontForUrl(
    MAIN_DOMAIN + "animation.html"
  );

  await playStateIsUpdatedDynamically(walker, animations);

  await target.destroy();
  gBrowser.removeCurrentTab();
});

async function playStateIsUpdatedDynamically(walker, animations) {
  info("Getting the test node (which runs a very long animation)");
  // The animation lasts for 100s, to avoid intermittents.
  const node = await walker.querySelector(walker.rootNode, ".long-animation");

  info("Getting the animation player front for this node");
  const [player] = await animations.getAnimationPlayersForNode(node);

  let state = await player.getCurrentState();
  is(
    state.playState,
    "running",
    "The playState is running while the animation is running"
  );

  info(
    "Change the animation's currentTime to be near the end and wait for " +
      "it to finish"
  );
  const onFinished = waitForAnimationPlayState(player, "finished");
  // Set the currentTime to 98s, knowing that the animation lasts for 100s.
  await animations.setCurrentTimes([player], 98 * 1000, false);
  state = await onFinished;
  is(
    state.playState,
    "finished",
    "The animation has ended and the state has been updated"
  );
  Assert.greater(
    state.currentTime,
    player.initialState.currentTime,
    "The currentTime has been updated"
  );
}

async function waitForAnimationPlayState(player, playState) {
  let state = {};
  while (state.playState !== playState) {
    state = await player.getCurrentState();
    await wait(500);
  }
  return state;
}

function wait(ms) {
  return new Promise(r => setTimeout(r, ms));
}
