/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Check the output of getAnimationPlayersForNode

add_task(async function() {
  const {client, walker, animations} =
    await initAnimationsFrontForUrl(MAIN_DOMAIN + "animation.html");

  await theRightNumberOfPlayersIsReturned(walker, animations);
  await playersCanBePausedAndResumed(walker, animations);

  await client.close();
  gBrowser.removeCurrentTab();
});

async function theRightNumberOfPlayersIsReturned(walker, animations) {
  let node = await walker.querySelector(walker.rootNode, ".not-animated");
  let players = await animations.getAnimationPlayersForNode(node);
  is(players.length, 0,
     "0 players were returned for the unanimated node");

  node = await walker.querySelector(walker.rootNode, ".simple-animation");
  players = await animations.getAnimationPlayersForNode(node);
  is(players.length, 1,
     "One animation player was returned");

  node = await walker.querySelector(walker.rootNode, ".multiple-animations");
  players = await animations.getAnimationPlayersForNode(node);
  is(players.length, 2,
     "Two animation players were returned");

  node = await walker.querySelector(walker.rootNode, ".transition");
  players = await animations.getAnimationPlayersForNode(node);
  is(players.length, 1,
     "One animation player was returned for the transitioned node");
}

async function playersCanBePausedAndResumed(walker, animations) {
  const node = await walker.querySelector(walker.rootNode, ".simple-animation");
  const [player] = await animations.getAnimationPlayersForNode(node);
  await player.ready();

  ok(player.initialState,
     "The player has an initialState");
  ok(player.getCurrentState,
     "The player has the getCurrentState method");
  is(player.initialState.playState, "running",
     "The animation is currently running");

  await player.pause();
  let state = await player.getCurrentState();
  is(state.playState, "paused",
     "The animation is now paused");

  await player.play();
  state = await player.getCurrentState();
  is(state.playState, "running",
     "The animation is now running again");
}
