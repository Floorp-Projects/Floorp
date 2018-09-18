/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Check that, even though the AnimationPlayerActor only sends the bits of its
// state that change, the front reconstructs the whole state everytime.

add_task(async function() {
  const {client, walker, animations} =
    await initAnimationsFrontForUrl(MAIN_DOMAIN + "animation.html");

  await playerHasCompleteStateAtAllTimes(walker, animations);

  await client.close();
  gBrowser.removeCurrentTab();
});

async function playerHasCompleteStateAtAllTimes(walker, animations) {
  const node = await walker.querySelector(walker.rootNode, ".simple-animation");
  const [player] = await animations.getAnimationPlayersForNode(node);
  await player.ready();

  // Get the list of state key names from the initialstate.
  const keys = Object.keys(player.initialState);

  // Get the state over and over again and check that the object returned
  // contains all keys.
  // Normally, only the currentTime will have changed in between 2 calls.
  for (let i = 0; i < 10; i++) {
    await player.refreshState();
    keys.forEach(key => {
      ok(typeof player.state[key] !== "undefined",
         "The state retrieved has key " + key);
    });
  }
}
