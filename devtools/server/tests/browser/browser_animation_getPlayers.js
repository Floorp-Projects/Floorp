/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Check the output of getAnimationPlayersForNode

add_task(function* () {
  let {client, walker, animations} =
    yield initAnimationsFrontForUrl(MAIN_DOMAIN + "animation.html");

  yield theRightNumberOfPlayersIsReturned(walker, animations);
  yield playersCanBePausedAndResumed(walker, animations);

  yield client.close();
  gBrowser.removeCurrentTab();
});

function* theRightNumberOfPlayersIsReturned(walker, animations) {
  let node = yield walker.querySelector(walker.rootNode, ".not-animated");
  let players = yield animations.getAnimationPlayersForNode(node);
  is(players.length, 0,
     "0 players were returned for the unanimated node");

  node = yield walker.querySelector(walker.rootNode, ".simple-animation");
  players = yield animations.getAnimationPlayersForNode(node);
  is(players.length, 1,
     "One animation player was returned");

  node = yield walker.querySelector(walker.rootNode, ".multiple-animations");
  players = yield animations.getAnimationPlayersForNode(node);
  is(players.length, 2,
     "Two animation players were returned");

  node = yield walker.querySelector(walker.rootNode, ".transition");
  players = yield animations.getAnimationPlayersForNode(node);
  is(players.length, 1,
     "One animation player was returned for the transitioned node");
}

function* playersCanBePausedAndResumed(walker, animations) {
  let node = yield walker.querySelector(walker.rootNode, ".simple-animation");
  let [player] = yield animations.getAnimationPlayersForNode(node);
  yield player.ready();

  ok(player.initialState,
     "The player has an initialState");
  ok(player.getCurrentState,
     "The player has the getCurrentState method");
  is(player.initialState.playState, "running",
     "The animation is currently running");

  yield player.pause();
  let state = yield player.getCurrentState();
  is(state.playState, "paused",
     "The animation is now paused");

  yield player.play();
  state = yield player.getCurrentState();
  is(state.playState, "running",
     "The animation is now running again");
}
