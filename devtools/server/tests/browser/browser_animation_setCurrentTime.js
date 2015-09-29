/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Check that a player's currentTime can be changed and that the AnimationsActor
// allows changing many players' currentTimes at once.

const {AnimationsFront} = require("devtools/server/actors/animation");
const {InspectorFront} = require("devtools/server/actors/inspector");

add_task(function*() {
  yield addTab(MAIN_DOMAIN + "animation.html");

  initDebuggerServer();
  let client = new DebuggerClient(DebuggerServer.connectPipe());
  let form = yield connectDebuggerClient(client);
  let inspector = InspectorFront(client, form);
  let walker = yield inspector.getWalker();
  let animations = AnimationsFront(client, form);

  yield testSetCurrentTime(walker, animations);
  yield testSetCurrentTimes(walker, animations);

  yield closeDebuggerClient(client);
  gBrowser.removeCurrentTab();
});

function* testSetCurrentTime(walker, animations) {
  info("Retrieve an animated node");
  let node = yield walker.querySelector(walker.rootNode, ".simple-animation");

  info("Retrieve the animation player for the node");
  let [player] = yield animations.getAnimationPlayersForNode(node);

  ok(player.setCurrentTime, "Player has the setCurrentTime method");

  info("Check that the setCurrentTime method can be called");
  // Note that we don't check that it sets the animation to the right time here,
  // this is too prone to intermittent failures, we'll do this later after
  // pausing the animation. Here we merely test that the method doesn't fail.
  yield player.setCurrentTime(player.initialState.currentTime + 1000);

  info("Pause the animation so we can really test if setCurrentTime works");
  yield player.pause();
  let pausedState = yield player.getCurrentState();

  info("Set the current time to currentTime + 5s");
  yield player.setCurrentTime(pausedState.currentTime + 5000);

  let updatedState1 = yield player.getCurrentState();
  is(Math.round(updatedState1.currentTime - pausedState.currentTime), 5000,
    "The currentTime was updated to +5s");

  info("Set the current time to currentTime - 2s");
  yield player.setCurrentTime(updatedState1.currentTime - 2000);
  let updatedState2 = yield player.getCurrentState();
  is(Math.round(updatedState2.currentTime - updatedState1.currentTime), -2000,
    "The currentTime was updated to -2s");
}

function* testSetCurrentTimes(walker, animations) {
  ok(animations.setCurrentTimes, "The AnimationsActor has the right method");

  info("Retrieve multiple animated nodes and their animation players");
  let node1 = yield walker.querySelector(walker.rootNode, ".simple-animation");
  let player1 = (yield animations.getAnimationPlayersForNode(node1))[0];
  let node2 = yield walker.querySelector(walker.rootNode, ".delayed-animation");
  let player2 = (yield animations.getAnimationPlayersForNode(node2))[0];

  info("Try to set multiple current times at once");
  yield animations.setCurrentTimes([player1, player2], 500, true);

  info("Get the states of both players and verify their correctness");
  let state1 = yield player1.getCurrentState();
  let state2 = yield player2.getCurrentState();
  is(state1.playState, "paused", "Player 1 is paused");
  is(state2.playState, "paused", "Player 2 is paused");
  is(state1.currentTime, 500, "Player 1 has the right currentTime");
  is(state2.currentTime, 500, "Player 2 has the right currentTime");
}
