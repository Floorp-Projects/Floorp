/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Check that a player's playbackRate can be changed, and that multiple players
// can have their rates changed at the same time.

add_task(async function() {
  const { target, walker, animations } = await initAnimationsFrontForUrl(
    MAIN_DOMAIN + "animation.html"
  );

  info("Retrieve an animated node");
  let node = await walker.querySelector(walker.rootNode, ".simple-animation");

  info("Retrieve the animation player for the node");
  const [player] = await animations.getAnimationPlayersForNode(node);

  info("Change the rate to 10");
  await animations.setPlaybackRates([player], 10);

  info("Query the state again");
  let state = await player.getCurrentState();
  is(state.playbackRate, 10, "The playbackRate was updated");

  info("Change the rate back to 1");
  await animations.setPlaybackRates([player], 1);

  info("Query the state again");
  state = await player.getCurrentState();
  is(state.playbackRate, 1, "The playbackRate was changed back");

  info("Retrieve several animation players and set their rates");
  node = await walker.querySelector(walker.rootNode, "body");
  const players = await animations.getAnimationPlayersForNode(node);

  info("Change all animations in <body> to .5 rate");
  await animations.setPlaybackRates(players, 0.5);

  info("Query their states and check they are correct");
  for (const animPlayer of players) {
    const animPlayerState = await animPlayer.getCurrentState();
    is(animPlayerState.playbackRate, 0.5, "The playbackRate was updated");
  }

  await target.destroy();
  gBrowser.removeCurrentTab();
});
