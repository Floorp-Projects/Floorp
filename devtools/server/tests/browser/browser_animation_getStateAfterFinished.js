/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Check that the right duration/iterationCount/delay are retrieved even when
// the node has multiple animations and one of them already ended before getting
// the player objects.
// See devtools/server/actors/animation.js |getPlayerIndex| for more
// information.

add_task(function* () {
  let {client, walker, animations} =
    yield initAnimationsFrontForUrl(MAIN_DOMAIN + "animation.html");

  info("Retrieve a non animated node");
  let node = yield walker.querySelector(walker.rootNode, ".not-animated");

  info("Apply the multiple-animations-2 class to start the animations");
  yield node.modifyAttributes([
    {attributeName: "class", newValue: "multiple-animations-2"}
  ]);

  info("Get the list of players, by the time this executes, the first, " +
       "short, animation should have ended.");
  let players = yield animations.getAnimationPlayersForNode(node);
  if (players.length === 3) {
    info("The short animation hasn't ended yet, wait for a bit.");
    // The animation lasts for 500ms, so 1000ms should do it.
    yield new Promise(resolve => setTimeout(resolve, 1000));

    info("And get the list again");
    players = yield animations.getAnimationPlayersForNode(node);
  }

  is(players.length, 2, "2 animations remain on the node");

  is(players[0].state.duration, 100000,
     "The duration of the first animation is correct");
  is(players[0].state.delay, 2000,
     "The delay of the first animation is correct");
  is(players[0].state.iterationCount, null,
     "The iterationCount of the first animation is correct");

  is(players[1].state.duration, 300000,
     "The duration of the second animation is correct");
  is(players[1].state.delay, 1000,
     "The delay of the second animation is correct");
  is(players[1].state.iterationCount, 100,
     "The iterationCount of the second animation is correct");

  yield client.close();
  gBrowser.removeCurrentTab();
});
