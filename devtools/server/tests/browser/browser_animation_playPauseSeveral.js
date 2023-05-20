/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Check that the AnimationsActor can pause/play a given list of animations at once.

// List of selectors that match "all" animated nodes in the test page.
// This list misses a bunch of animated nodes on purpose. Only the ones that
// have infinite animations are listed. This is done to avoid intermittents
// caused when finite animations are already done playing by the time the test
// runs.
const ALL_ANIMATED_NODES = [
  ".simple-animation",
  ".multiple-animations",
  ".delayed-animation",
];

add_task(async function () {
  const { target, walker, animations } = await initAnimationsFrontForUrl(
    MAIN_DOMAIN + "animation.html"
  );

  info("Pause all animations in the test document");
  await toggleAndCheckStates(walker, animations, ALL_ANIMATED_NODES, "paused");

  info("Play all animations in the test document");
  await toggleAndCheckStates(walker, animations, ALL_ANIMATED_NODES, "running");

  await target.destroy();
  gBrowser.removeCurrentTab();
});

async function toggleAndCheckStates(walker, animations, selectors, playState) {
  info(
    "Checking the playState of all the nodes that have infinite running " +
      "animations"
  );

  for (const selector of selectors) {
    const players = await getPlayersFor(walker, animations, selector);

    if (playState === "paused") {
      await animations.pauseSome(players);
    } else {
      await animations.playSome(players);
    }

    info("Getting the AnimationPlayerFront for node " + selector);
    const player = players[0];
    await checkPlayState(player, selector, playState);
  }
}

async function getPlayersFor(walker, animations, selector) {
  const node = await walker.querySelector(walker.rootNode, selector);
  return animations.getAnimationPlayersForNode(node);
}

async function checkPlayState(player, selector, expectedState) {
  const state = await player.getCurrentState();
  is(
    state.playState,
    expectedState,
    "The playState of node " + selector + " is " + expectedState
  );
}
