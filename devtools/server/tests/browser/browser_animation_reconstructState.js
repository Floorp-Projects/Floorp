/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Check that, even though the AnimationPlayerActor only sends the bits of its
// state that change, the front reconstructs the whole state everytime.

add_task(function*() {
  let {client, walker, animations} =
    yield initAnimationsFrontForUrl(MAIN_DOMAIN + "animation.html");

  yield playerHasCompleteStateAtAllTimes(walker, animations);

  yield closeDebuggerClient(client);
  gBrowser.removeCurrentTab();
});

function* playerHasCompleteStateAtAllTimes(walker, animations) {
  let node = yield walker.querySelector(walker.rootNode, ".simple-animation");
  let [player] = yield animations.getAnimationPlayersForNode(node);
  yield player.ready();

  // Get the list of state key names from the initialstate.
  let keys = Object.keys(player.initialState);

  // Get the state over and over again and check that the object returned
  // contains all keys.
  // Normally, only the currentTime will have changed in between 2 calls.
  for (let i = 0; i < 10; i++) {
    let state = yield player.getCurrentState();
    keys.forEach(key => {
      ok(typeof state[key] !== "undefined",
         "The state retrieved has key " + key);
    });
  }
}
