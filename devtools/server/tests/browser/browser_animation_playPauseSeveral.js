/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Check that the AnimationsActor can pause/play all animations at once, and
// check that it can also pause/play a given list of animations at once.

// List of selectors that match "all" animated nodes in the test page.
// This list misses a bunch of animated nodes on purpose. Only the ones that
// have infinite animations are listed. This is done to avoid intermittents
// caused when finite animations are already done playing by the time the test
// runs.
const ALL_ANIMATED_NODES = [".simple-animation", ".multiple-animations",
                            ".delayed-animation"];

add_task(async function() {
  const {client, walker, animations} =
    await initAnimationsFrontForUrl(MAIN_DOMAIN + "animation.html");

  info("Pause all animations in the test document");
  await animations.pauseAll();
  await checkStates(walker, animations, ALL_ANIMATED_NODES, "paused");

  info("Play all animations in the test document");
  await animations.playAll();
  await checkStates(walker, animations, ALL_ANIMATED_NODES, "running");

  info("Pause all animations in the test document using toggleAll");
  await animations.toggleAll();
  await checkStates(walker, animations, ALL_ANIMATED_NODES, "paused");

  info("Play all animations in the test document using toggleAll");
  await animations.toggleAll();
  await checkStates(walker, animations, ALL_ANIMATED_NODES, "running");

  info("Play all animations from multiple animated node using toggleSeveral");
  const players = await getPlayersFor(walker, animations,
                                   [".multiple-animations"]);
  is(players.length, 2, "Node has 2 animation players");
  await animations.toggleSeveral(players, false);
  let state1 = await players[0].getCurrentState();
  is(state1.playState, "running",
    "The playState of the first player is running");
  let state2 = await players[1].getCurrentState();
  is(state2.playState, "running",
    "The playState of the second player is running");

  info("Pause one animation from a multiple animated node using toggleSeveral");
  await animations.toggleSeveral([players[0]], true);
  state1 = await players[0].getCurrentState();
  is(state1.playState, "paused", "The playState of the first player is paused");
  state2 = await players[1].getCurrentState();
  is(state2.playState, "running",
    "The playState of the second player is running");

  info("Play the same animation");
  await animations.toggleSeveral([players[0]], false);
  state1 = await players[0].getCurrentState();
  is(state1.playState, "running",
    "The playState of the first player is running");
  state2 = await players[1].getCurrentState();
  is(state2.playState, "running",
    "The playState of the second player is running");

  await client.close();
  gBrowser.removeCurrentTab();
});

async function checkStates(walker, animations, selectors, playState) {
  info("Checking the playState of all the nodes that have infinite running " +
       "animations");

  for (const selector of selectors) {
    info("Getting the AnimationPlayerFront for node " + selector);
    const [player] = await getPlayersFor(walker, animations, selector);
    await player.ready();
    await checkPlayState(player, selector, playState);
  }
}

async function getPlayersFor(walker, animations, selector) {
  const node = await walker.querySelector(walker.rootNode, selector);
  return animations.getAnimationPlayersForNode(node);
}

async function checkPlayState(player, selector, expectedState) {
  const state = await player.getCurrentState();
  is(state.playState, expectedState,
    "The playState of node " + selector + " is " + expectedState);
}
