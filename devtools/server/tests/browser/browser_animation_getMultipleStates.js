/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Check that the duration, iterationCount and delay are retrieved correctly for
// multiple animations.

add_task(function*() {
  let {client, walker, animations} =
    yield initAnimationsFrontForUrl(MAIN_DOMAIN + "animation.html");

  yield playerHasAnInitialState(walker, animations);

  yield closeDebuggerClient(client);
  gBrowser.removeCurrentTab();
});

function* playerHasAnInitialState(walker, animations) {
  let state = yield getAnimationStateForNode(walker, animations,
    ".delayed-multiple-animations", 0);

  ok(state.duration, 500,
     "The duration of the first animation is correct");
  ok(state.iterationCount, 10,
     "The iterationCount of the first animation is correct");
  ok(state.delay, 1000,
     "The delay of the first animation is correct");

  state = yield getAnimationStateForNode(walker, animations,
    ".delayed-multiple-animations", 1);

  ok(state.duration, 1000,
     "The duration of the secon animation is correct");
  ok(state.iterationCount, 30,
     "The iterationCount of the secon animation is correct");
  ok(state.delay, 750,
     "The delay of the secon animation is correct");
}

function* getAnimationStateForNode(walker, animations, selector, playerIndex) {
  let node = yield walker.querySelector(walker.rootNode, selector);
  let players = yield animations.getAnimationPlayersForNode(node);
  let player = players[playerIndex];
  yield player.ready();
  let state = yield player.getCurrentState();
  return state;
}
