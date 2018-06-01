/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Check that the AnimationsActor can pause/play all animations even those
// within iframes.

const URL = MAIN_DOMAIN + "animation.html";

add_task(async function() {
  info("Creating a test document with 2 iframes containing animated nodes");

  const {client, walker, animations} = await initAnimationsFrontForUrl(
    "data:text/html;charset=utf-8," +
    "<iframe id='i1' src='" + URL + "'></iframe>" +
    "<iframe id='i2' src='" + URL + "'></iframe>");

  info("Getting the 2 iframe container nodes and animated nodes in them");
  const nodeInFrame1 = await getNodeInFrame(walker, "#i1", ".simple-animation");
  const nodeInFrame2 = await getNodeInFrame(walker, "#i2", ".simple-animation");

  info("Pause all animations in the test document");
  await animations.pauseAll();
  await checkState(animations, nodeInFrame1, "paused");
  await checkState(animations, nodeInFrame2, "paused");

  info("Play all animations in the test document");
  await animations.playAll();
  await checkState(animations, nodeInFrame1, "running");
  await checkState(animations, nodeInFrame2, "running");

  await client.close();
  gBrowser.removeCurrentTab();
});

async function checkState(animations, nodeFront, playState) {
  info("Getting the AnimationPlayerFront for the test node");
  const [player] = await animations.getAnimationPlayersForNode(nodeFront);
  await player.ready;
  const state = await player.getCurrentState();
  is(state.playState, playState,
     "The playState of the test node is " + playState);
}

async function getNodeInFrame(walker, frameSelector, nodeSelector) {
  const iframe = await walker.querySelector(walker.rootNode, frameSelector);
  const {nodes} = await walker.children(iframe);
  return walker.querySelector(nodes[0], nodeSelector);
}
