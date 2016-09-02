/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Check the animation player's initial state

add_task(function* () {
  let {client, walker, animations} =
    yield initAnimationsFrontForUrl(MAIN_DOMAIN + "animation.html");

  yield playerHasAnInitialState(walker, animations);
  yield playerStateIsCorrect(walker, animations);

  yield client.close();
  gBrowser.removeCurrentTab();
});

function* playerHasAnInitialState(walker, animations) {
  let node = yield walker.querySelector(walker.rootNode, ".simple-animation");
  let [player] = yield animations.getAnimationPlayersForNode(node);

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
  ok("isRunningOnCompositor" in player.initialState,
     "Player's state has isRunningOnCompositor");
  ok("type" in player.initialState, "Player's state has type");
  ok("documentCurrentTime" in player.initialState,
     "Player's state has documentCurrentTime");
}

function* playerStateIsCorrect(walker, animations) {
  info("Checking the state of the simple animation");

  let state = yield getAnimationStateForNode(walker, animations,
                                             ".simple-animation", 0);
  is(state.name, "move", "Name is correct");
  is(state.duration, 200000, "Duration is correct");
  // null = infinite count
  is(state.iterationCount, null, "Iteration count is correct");
  is(state.playState, "running", "PlayState is correct");
  is(state.playbackRate, 1, "PlaybackRate is correct");
  is(state.type, "cssanimation", "Type is correct");

  info("Checking the state of the transition");

  state = yield getAnimationStateForNode(walker, animations, ".transition", 0);
  is(state.name, "width", "Transition name matches transition property");
  is(state.duration, 500000, "Transition duration is correct");
  // transitions run only once
  is(state.iterationCount, 1, "Transition iteration count is correct");
  is(state.playState, "running", "Transition playState is correct");
  is(state.playbackRate, 1, "Transition playbackRate is correct");
  is(state.type, "csstransition", "Transition type is correct");

  info("Checking the state of one of multiple animations on a node");

  // Checking the 2nd player
  state = yield getAnimationStateForNode(walker, animations,
                                         ".multiple-animations", 1);
  is(state.name, "glow", "The 2nd animation's name is correct");
  is(state.duration, 100000, "The 2nd animation's duration is correct");
  is(state.iterationCount, 5, "The 2nd animation's iteration count is correct");
  is(state.playState, "running", "The 2nd animation's playState is correct");
  is(state.playbackRate, 1, "The 2nd animation's playbackRate is correct");

  info("Checking the state of an animation with delay");

  state = yield getAnimationStateForNode(walker, animations,
                                         ".delayed-animation", 0);
  is(state.delay, 5000, "The animation delay is correct");

  info("Checking the state of an transition with delay");

  state = yield getAnimationStateForNode(walker, animations,
                                         ".delayed-transition", 0);
  is(state.delay, 3000, "The transition delay is correct");
}

function* getAnimationStateForNode(walker, animations, nodeSelector, index) {
  let node = yield walker.querySelector(walker.rootNode, nodeSelector);
  let players = yield animations.getAnimationPlayersForNode(node);
  let player = players[index];
  yield player.ready();
  let state = yield player.getCurrentState();
  return state;
}
