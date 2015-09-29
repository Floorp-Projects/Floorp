/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Check the output of getAnimationPlayersForNode

const {AnimationsFront} = require("devtools/server/actors/animation");
const {InspectorFront} = require("devtools/server/actors/inspector");

add_task(function*() {
  let doc = yield addTab(MAIN_DOMAIN + "animation.html");

  initDebuggerServer();
  let client = new DebuggerClient(DebuggerServer.connectPipe());
  let form = yield connectDebuggerClient(client);
  let inspector = InspectorFront(client, form);
  let walker = yield inspector.getWalker();
  let front = AnimationsFront(client, form);

  yield theRightNumberOfPlayersIsReturned(walker, front);
  yield playersCanBePausedAndResumed(walker, front);

  yield closeDebuggerClient(client);
  gBrowser.removeCurrentTab();
});

function* theRightNumberOfPlayersIsReturned(walker, front) {
  let node = yield walker.querySelector(walker.rootNode, ".not-animated");
  let players = yield front.getAnimationPlayersForNode(node);
  is(players.length, 0, "0 players were returned for the unanimated node");

  node = yield walker.querySelector(walker.rootNode, ".simple-animation");
  players = yield front.getAnimationPlayersForNode(node);
  is(players.length, 1, "One animation player was returned");

  node = yield walker.querySelector(walker.rootNode, ".multiple-animations");
  players = yield front.getAnimationPlayersForNode(node);
  is(players.length, 2, "Two animation players were returned");

  node = yield walker.querySelector(walker.rootNode, ".transition");
  players = yield front.getAnimationPlayersForNode(node);
  is(players.length, 1, "One animation player was returned for the transitioned node");
}

function* playersCanBePausedAndResumed(walker, front) {
  let node = yield walker.querySelector(walker.rootNode, ".simple-animation");
  let [player] = yield front.getAnimationPlayersForNode(node);
  yield player.ready();

  ok(player.initialState, "The player has an initialState");
  ok(player.getCurrentState, "The player has the getCurrentState method");
  is(player.initialState.playState, "running", "The animation is currently running");

  yield player.pause();
  let state = yield player.getCurrentState();
  is(state.playState, "paused", "The animation is now paused");

  yield player.play();
  state = yield player.getCurrentState();
  is(state.playState, "running", "The animation is now running again");
}
