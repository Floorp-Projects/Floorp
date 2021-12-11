/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Check that the duration, iterationCount and delay are retrieved correctly for
// multiple animations.

add_task(async function() {
  const { target, walker, animations } = await initAnimationsFrontForUrl(
    MAIN_DOMAIN + "animation.html"
  );

  await playerHasAnInitialState(walker, animations);

  await target.destroy();
  gBrowser.removeCurrentTab();
});

async function playerHasAnInitialState(walker, animations) {
  let state = await getAnimationStateForNode(
    walker,
    animations,
    ".delayed-multiple-animations",
    0
  );

  is(state.duration, 500, "The duration of the first animation is correct");
  is(
    state.iterationCount,
    10,
    "The iterationCount of the first animation is correct"
  );
  is(state.delay, 1000, "The delay of the first animation is correct");

  state = await getAnimationStateForNode(
    walker,
    animations,
    ".delayed-multiple-animations",
    1
  );

  is(state.duration, 1000, "The duration of the second animation is correct");
  is(
    state.iterationCount,
    30,
    "The iterationCount of the second animation is correct"
  );
  is(state.delay, 750, "The delay of the second animation is correct");
}

async function getAnimationStateForNode(
  walker,
  animations,
  selector,
  playerIndex
) {
  const node = await walker.querySelector(walker.rootNode, selector);
  const players = await animations.getAnimationPlayersForNode(node);
  const player = players[playerIndex];
  const state = await player.getCurrentState();
  return state;
}
