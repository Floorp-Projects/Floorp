/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Check that the AnimationsActor can pause/play all animations even those
// within iframes.

const URL = MAIN_DOMAIN + "animation.html";

add_task(function* () {
  info("Creating a test document with 2 iframes containing animated nodes");

  let {client, walker, animations} = yield initAnimationsFrontForUrl(
    "data:text/html;charset=utf-8," +
    "<iframe id='i1' src='" + URL + "'></iframe>" +
    "<iframe id='i2' src='" + URL + "'></iframe>");

  info("Getting the 2 iframe container nodes and animated nodes in them");
  let nodeInFrame1 = yield getNodeInFrame(walker, "#i1", ".simple-animation");
  let nodeInFrame2 = yield getNodeInFrame(walker, "#i2", ".simple-animation");

  info("Pause all animations in the test document");
  yield animations.pauseAll();
  yield checkState(animations, nodeInFrame1, "paused");
  yield checkState(animations, nodeInFrame2, "paused");

  info("Play all animations in the test document");
  yield animations.playAll();
  yield checkState(animations, nodeInFrame1, "running");
  yield checkState(animations, nodeInFrame2, "running");

  yield closeDebuggerClient(client);
  gBrowser.removeCurrentTab();
});

function* checkState(animations, nodeFront, playState) {
  info("Getting the AnimationPlayerFront for the test node");
  let [player] = yield animations.getAnimationPlayersForNode(nodeFront);
  yield player.ready;
  let state = yield player.getCurrentState();
  is(state.playState, playState,
     "The playState of the test node is " + playState);
}

function* getNodeInFrame(walker, frameSelector, nodeSelector) {
  let iframe = yield walker.querySelector(walker.rootNode, frameSelector);
  let {nodes} = yield walker.children(iframe);
  return walker.querySelector(nodes[0], nodeSelector);
}
