/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Check the output of getAnimationPlayersForNode

add_task(async function () {
  const { target, walker, animations } = await initAnimationsFrontForUrl(
    MAIN_DOMAIN + "animation.html"
  );

  await theRightNumberOfPlayersIsReturned(walker, animations);

  await target.destroy();
  gBrowser.removeCurrentTab();
});

async function theRightNumberOfPlayersIsReturned(walker, animations) {
  let node = await walker.querySelector(walker.rootNode, ".not-animated");
  let players = await animations.getAnimationPlayersForNode(node);
  is(players.length, 0, "0 players were returned for the unanimated node");

  node = await walker.querySelector(walker.rootNode, ".simple-animation");
  players = await animations.getAnimationPlayersForNode(node);
  is(players.length, 1, "One animation player was returned");

  node = await walker.querySelector(walker.rootNode, ".multiple-animations");
  players = await animations.getAnimationPlayersForNode(node);
  is(players.length, 2, "Two animation players were returned");

  node = await walker.querySelector(walker.rootNode, ".transition");
  players = await animations.getAnimationPlayersForNode(node);
  is(
    players.length,
    1,
    "One animation player was returned for the transitioned node"
  );
}
