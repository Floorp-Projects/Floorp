/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */
/* eslint-disable mozilla/no-arbitrary-setTimeout */

"use strict";

// Check that the right duration/iterationCount/delay are retrieved even when
// the node has multiple animations and one of them already ended before getting
// the player objects.
// See devtools/server/actors/animation.js |getPlayerIndex| for more
// information.

add_task(async function() {
  const { target, walker, animations } = await initAnimationsFrontForUrl(
    MAIN_DOMAIN + "animation.html"
  );

  info("Retrieve a non animated node");
  const node = await walker.querySelector(walker.rootNode, ".not-animated");

  info("Apply the multiple-animations-2 class to start the animations");
  await node.modifyAttributes([
    { attributeName: "class", newValue: "multiple-animations-2" },
  ]);

  info(
    "Get the list of players, by the time this executes, the first, " +
      "short, animation should have ended."
  );
  let players = await animations.getAnimationPlayersForNode(node);
  if (players.length === 3) {
    info("The short animation hasn't ended yet, wait for a bit.");
    // The animation lasts for 500ms, so 1000ms should do it.
    await new Promise(resolve => setTimeout(resolve, 1000));

    info("And get the list again");
    players = await animations.getAnimationPlayersForNode(node);
  }

  is(players.length, 2, "2 animations remain on the node");

  is(
    players[0].state.duration,
    100000,
    "The duration of the first animation is correct"
  );
  is(
    players[0].state.delay,
    2000,
    "The delay of the first animation is correct"
  );
  is(
    players[0].state.iterationCount,
    null,
    "The iterationCount of the first animation is correct"
  );

  is(
    players[1].state.duration,
    300000,
    "The duration of the second animation is correct"
  );
  is(
    players[1].state.delay,
    1000,
    "The delay of the second animation is correct"
  );
  is(
    players[1].state.iterationCount,
    100,
    "The iterationCount of the second animation is correct"
  );

  await target.destroy();
  gBrowser.removeCurrentTab();
});
