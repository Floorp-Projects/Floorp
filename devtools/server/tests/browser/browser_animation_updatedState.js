/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Check the animation player's updated state

add_task(function*() {
  let {client, walker, animations} =
    yield initAnimationsFrontForUrl(MAIN_DOMAIN + "animation.html");

  yield playStateIsUpdatedDynamically(walker, animations);

  yield closeDebuggerClient(client);
  gBrowser.removeCurrentTab();
});

function* playStateIsUpdatedDynamically(walker, animations) {
  let node = yield walker.querySelector(walker.rootNode, ".short-animation");

  // Restart the animation to make sure we can get the player (it might already
  // be finished by now). Do this by toggling the class and forcing a sync
  // reflow using the CPOW.
  let cpow = content.document.querySelector(".short-animation");
  cpow.classList.remove("short-animation");
  let reflow = cpow.offsetWidth;
  cpow.classList.add("short-animation");

  let [player] = yield animations.getAnimationPlayersForNode(node);

  yield player.ready();
  let state = yield player.getCurrentState();

  is(state.playState, "running",
    "The playState is running while the transition is running");

  info("Wait until the animation stops (more than 1000ms)");
  // Waiting 1.5sec for good measure
  yield wait(1500);

  state = yield player.getCurrentState();
  is(state.playState, "finished",
    "The animation has ended and the state has been updated");
  ok(state.currentTime > player.initialState.currentTime,
    "The currentTime has been updated");
}

function wait(ms) {
  return new Promise(resolve => {
    setTimeout(resolve, ms);
  });
}
