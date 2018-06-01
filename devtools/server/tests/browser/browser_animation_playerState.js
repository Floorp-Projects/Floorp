/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Check the animation player's initial state

add_task(async function() {
  const {client, walker, animations} =
    await initAnimationsFrontForUrl(MAIN_DOMAIN + "animation.html");

  await playerHasAnInitialState(walker, animations);
  await playerStateIsCorrect(walker, animations);

  await client.close();
  gBrowser.removeCurrentTab();
});

async function playerHasAnInitialState(walker, animations) {
  const node = await walker.querySelector(walker.rootNode, ".simple-animation");
  const [player] = await animations.getAnimationPlayersForNode(node);

  ok(player.initialState, "The player front has an initial state");
  ok("startTime" in player.initialState, "Player's state has startTime");
  ok("currentTime" in player.initialState, "Player's state has currentTime");
  ok("playState" in player.initialState, "Player's state has playState");
  ok("playbackRate" in player.initialState, "Player's state has playbackRate");
  ok("name" in player.initialState, "Player's state has name");
  ok("duration" in player.initialState, "Player's state has duration");
  ok("delay" in player.initialState, "Player's state has delay");
  ok("iterationCount" in player.initialState,
     "Player's state has iterationCount");
  ok("fill" in player.initialState, "Player's state has fill");
  ok("easing" in player.initialState, "Player's state has easing");
  ok("direction" in player.initialState, "Player's state has direction");
  ok("isRunningOnCompositor" in player.initialState,
     "Player's state has isRunningOnCompositor");
  ok("type" in player.initialState, "Player's state has type");
  ok("documentCurrentTime" in player.initialState,
     "Player's state has documentCurrentTime");
}

async function playerStateIsCorrect(walker, animations) {
  info("Checking the state of the simple animation");

  let player = await getAnimationPlayerForNode(walker, animations,
                                               ".simple-animation", 0);
  let state = await player.getCurrentState();
  is(state.name, "move", "Name is correct");
  is(state.duration, 200000, "Duration is correct");
  // null = infinite count
  is(state.iterationCount, null, "Iteration count is correct");
  is(state.fill, "none", "Fill is correct");
  is(state.easing, "linear", "Easing is correct");
  is(state.direction, "normal", "Direction is correct");
  is(state.playState, "running", "PlayState is correct");
  is(state.playbackRate, 1, "PlaybackRate is correct");
  is(state.type, "cssanimation", "Type is correct");

  info("Checking the state of the transition");

  player =
    await getAnimationPlayerForNode(walker, animations, ".transition", 0);
  state = await player.getCurrentState();
  is(state.name, "width", "Transition name matches transition property");
  is(state.duration, 500000, "Transition duration is correct");
  // transitions run only once
  is(state.iterationCount, 1, "Transition iteration count is correct");
  is(state.fill, "backwards", "Transition fill is correct");
  is(state.easing, "linear", "Transition easing is correct");
  is(state.direction, "normal", "Transition direction is correct");
  is(state.playState, "running", "Transition playState is correct");
  is(state.playbackRate, 1, "Transition playbackRate is correct");
  is(state.type, "csstransition", "Transition type is correct");
  // chech easing in keyframe
  let keyframes = await player.getFrames();
  is(keyframes.length, 2, "Transition length of keyframe is correct");
  is(keyframes[0].easing,
     "ease-out", "Transition kerframes's easing is correct");

  info("Checking the state of one of multiple animations on a node");

  // Checking the 2nd player
  player = await getAnimationPlayerForNode(walker, animations,
                                           ".multiple-animations", 1);
  state = await player.getCurrentState();
  is(state.name, "glow", "The 2nd animation's name is correct");
  is(state.duration, 100000, "The 2nd animation's duration is correct");
  is(state.iterationCount, 5, "The 2nd animation's iteration count is correct");
  is(state.fill, "both", "The 2nd animation's fill is correct");
  is(state.easing, "linear", "The 2nd animation's easing is correct");
  is(state.direction, "reverse", "The 2nd animation's direction is correct");
  is(state.playState, "running", "The 2nd animation's playState is correct");
  is(state.playbackRate, 1, "The 2nd animation's playbackRate is correct");
  // chech easing in keyframe
  keyframes = await player.getFrames();
  is(keyframes.length, 2, "The 2nd animation's length of keyframe is correct");
  is(keyframes[0].easing,
     "ease-out", "The 2nd animation's easing of kerframes is correct");

  info("Checking the state of an animation with delay");

  player = await getAnimationPlayerForNode(walker, animations,
                                           ".delayed-animation", 0);
  state = await player.getCurrentState();
  is(state.delay, 5000, "The animation delay is correct");

  info("Checking the state of an transition with delay");

  player = await getAnimationPlayerForNode(walker, animations,
                                           ".delayed-transition", 0);
  state = await player.getCurrentState();
  is(state.delay, 3000, "The transition delay is correct");
}

async function getAnimationPlayerForNode(walker, animations, nodeSelector, index) {
  const node = await walker.querySelector(walker.rootNode, nodeSelector);
  const players = await animations.getAnimationPlayersForNode(node);
  const player = players[index];
  await player.ready();
  return player;
}
