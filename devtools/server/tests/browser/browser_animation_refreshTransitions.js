/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// When a transition finishes, no "removed" event is sent because it may still
// be used, but when it restarts again (transitions back), then a new
// AnimationPlayerFront should be sent, and the old one should be removed.

add_task(function*() {
  let {client, walker, animations} =
    yield initAnimationsFrontForUrl(MAIN_DOMAIN + "animation.html");

  info("Retrieve the test node");
  let node = yield walker.querySelector(walker.rootNode, ".all-transitions");

  info("Retrieve the animation players for the node");
  let players = yield animations.getAnimationPlayersForNode(node);
  is(players.length, 0, "The node has no animation players yet");

  info("Play a transition by adding the expand class, wait for mutations");
  let onMutations = expectMutationEvents(animations, 2);
  let cpow = content.document.querySelector(".all-transitions");
  cpow.classList.add("expand");
  let reportedMutations = yield onMutations;

  is(reportedMutations.length, 2, "2 mutation events were received");
  is(reportedMutations[0].type, "added", "The first event was 'added'");
  is(reportedMutations[1].type, "added", "The second event was 'added'");

  info("Wait for the transitions to be finished");
  yield waitForEnd(reportedMutations[0].player);
  yield waitForEnd(reportedMutations[1].player);

  info("Play the transition back by removing the class, wait for mutations");
  onMutations = expectMutationEvents(animations, 4);
  cpow.classList.remove("expand");
  reportedMutations = yield onMutations;

  is(reportedMutations.length, 4, "4 new mutation events were received");
  is(reportedMutations.filter(m => m.type === "removed").length, 2,
    "2 'removed' events were sent (for the old transitions)");
  is(reportedMutations.filter(m => m.type === "added").length, 2,
    "2 'added' events were sent (for the new transitions)");

  yield closeDebuggerClient(client);
  gBrowser.removeCurrentTab();
});

function expectMutationEvents(animationsFront, nbOfEvents) {
  return new Promise(resolve => {
    let reportedMutations = [];
    function onMutations(mutations) {
      reportedMutations = [...reportedMutations, ...mutations];
      info("Received " + reportedMutations.length + " mutation events, " +
           "expecting " + nbOfEvents);
      if (reportedMutations.length === nbOfEvents) {
        animationsFront.off("mutations", onMutations);
        resolve(reportedMutations);
      }
    }

    info("Start listening for mutation events from the AnimationsFront");
    animationsFront.on("mutations", onMutations);
  });
}

function* waitForEnd(animationFront) {
  let playState;
  while (playState !== "finished") {
    let state = yield animationFront.getCurrentState();
    playState = state.playState;
    info("Wait for transition " + animationFront.state.name +
         " to finish, playState=" + playState);
  }
}
