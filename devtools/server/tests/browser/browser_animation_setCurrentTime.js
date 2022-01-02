/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Check that the AnimationsActor allows changing many players' currentTimes at once.

add_task(async function() {
  const { target, walker, animations } = await initAnimationsFrontForUrl(
    MAIN_DOMAIN + "animation.html"
  );

  await testSetCurrentTimes(walker, animations);

  await target.destroy();
  gBrowser.removeCurrentTab();
});

async function testSetCurrentTimes(walker, animations) {
  ok(animations.setCurrentTimes, "The AnimationsActor has the right method");

  info("Retrieve multiple animated node and its animation players");

  const nodeMulti = await walker.querySelector(
    walker.rootNode,
    ".multiple-animations"
  );
  const players = await animations.getAnimationPlayersForNode(nodeMulti);

  ok(players.length > 1, "Node has more than 1 animation player");

  info("Try to set multiple current times at once");
  // Assume that all animations were created at same time.
  const createdTime = players[1].state.createdTime;
  await animations.setCurrentTimes(players, createdTime + 500, true);

  info("Get the states of players and verify their correctness");
  for (let i = 0; i < players.length; i++) {
    const state = await players[i].getCurrentState();
    is(state.playState, "paused", `Player ${i + 1} is paused`);
    is(
      parseInt(state.currentTime.toPrecision(4), 10),
      500,
      `Player ${i + 1} has the right currentTime`
    );
  }
}
