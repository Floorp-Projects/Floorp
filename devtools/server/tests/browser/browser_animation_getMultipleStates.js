/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Check that the duration, iterationCount and delay are retrieved correctly for
// multiple animations.

add_task(async function() {
  const {client, walker, animations} =
    await initAnimationsFrontForUrl(MAIN_DOMAIN + "animation.html");

  await playerHasAnInitialState(walker, animations);

  await client.close();
  gBrowser.removeCurrentTab();
});

async function playerHasAnInitialState(walker, animations) {
  let state = await getAnimationStateForNode(walker, animations,
    ".delayed-multiple-animations", 0);

  ok(state.duration, 50000,
     "The duration of the first animation is correct");
  ok(state.iterationCount, 10,
     "The iterationCount of the first animation is correct");
  ok(state.delay, 1000,
     "The delay of the first animation is correct");

  state = await getAnimationStateForNode(walker, animations,
    ".delayed-multiple-animations", 1);

  ok(state.duration, 100000,
     "The duration of the secon animation is correct");
  ok(state.iterationCount, 30,
     "The iterationCount of the secon animation is correct");
  ok(state.delay, 750,
     "The delay of the secon animation is correct");
}

async function getAnimationStateForNode(walker, animations, selector, playerIndex) {
  const node = await walker.querySelector(walker.rootNode, selector);
  const players = await animations.getAnimationPlayersForNode(node);
  const player = players[playerIndex];
  await player.ready();
  const state = await player.getCurrentState();
  return state;
}
