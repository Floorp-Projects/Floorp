/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Check that a player's currentTime can be changed and that the AnimationsActor
// allows changing many players' currentTimes at once.

add_task(async function() {
  const {client, walker, animations} =
    await initAnimationsFrontForUrl(MAIN_DOMAIN + "animation.html");

  await testSetCurrentTime(walker, animations);
  await testSetCurrentTimes(walker, animations);

  await client.close();
  gBrowser.removeCurrentTab();
});

async function testSetCurrentTime(walker, animations) {
  info("Retrieve an animated node");
  const node = await walker.querySelector(walker.rootNode, ".simple-animation");

  info("Retrieve the animation player for the node");
  const [player] = await animations.getAnimationPlayersForNode(node);

  ok(player.setCurrentTime, "Player has the setCurrentTime method");

  info("Check that the setCurrentTime method can be called");
  // Note that we don't check that it sets the animation to the right time here,
  // this is too prone to intermittent failures, we'll do this later after
  // pausing the animation. Here we merely test that the method doesn't fail.
  await player.setCurrentTime(player.initialState.currentTime + 1000);

  info("Pause the animation so we can really test if setCurrentTime works");
  await player.pause();
  const pausedState = await player.getCurrentState();

  info("Set the current time to currentTime + 5s");
  await player.setCurrentTime(pausedState.currentTime + 5000);

  const updatedState1 = await player.getCurrentState();
  is(Math.round(updatedState1.currentTime - pausedState.currentTime), 5000,
    "The currentTime was updated to +5s");

  info("Set the current time to currentTime - 2s");
  await player.setCurrentTime(updatedState1.currentTime - 2000);
  const updatedState2 = await player.getCurrentState();
  is(Math.round(updatedState2.currentTime - updatedState1.currentTime), -2000,
    "The currentTime was updated to -2s");
}

async function testSetCurrentTimes(walker, animations) {
  ok(animations.setCurrentTimes, "The AnimationsActor has the right method");

  info("Retrieve multiple animated node and its animation players");

  const nodeMulti = await walker.querySelector(walker.rootNode,
    ".multiple-animations");
  const players = (await animations.getAnimationPlayersForNode(nodeMulti));

  ok(players.length > 1, "Node has more than 1 animation player");

  info("Try to set multiple current times at once");
  await animations.setCurrentTimes(players, 500, true);

  info("Get the states of players and verify their correctness");
  for (let i = 0; i < players.length; i++) {
    const state = await players[i].getCurrentState();
    is(state.playState, "paused", `Player ${i + 1} is paused`);
    is(state.currentTime, 500, `Player ${i + 1} has the right currentTime`);
  }
}
