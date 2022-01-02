/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Check that the AnimationsActor can pause/play all animations even those
// within iframes.

const URL = MAIN_DOMAIN + "animation.html";

// Import inspector's shared head.
Services.scriptloader.loadSubScript(
  "chrome://mochitests/content/browser/devtools/client/inspector/test/shared-head.js",
  this
);

add_task(async function() {
  info("Creating a test document with 2 iframes containing animated nodes");

  const { inspector, target } = await initAnimationsFrontForUrl(
    "data:text/html;charset=utf-8," +
      "<iframe id='i1' src='" +
      URL +
      "'></iframe>" +
      "<iframe id='i2' src='" +
      URL +
      "'></iframe>"
  );

  info("Getting the 2 iframe container nodes and animated nodes in them");
  const nodeInFrame1 = await getNodeFrontInFrames(
    ["#i1", ".simple-animation"],
    inspector
  );
  const nodeInFrame2 = await getNodeFrontInFrames(
    ["#i2", ".simple-animation"],
    inspector
  );

  info("Pause all animations in the test document");
  await toggleAndCheckStates(nodeInFrame1, "paused");
  await toggleAndCheckStates(nodeInFrame2, "paused");

  info("Play all animations in the test document");
  await toggleAndCheckStates(nodeInFrame1, "running");
  await toggleAndCheckStates(nodeInFrame2, "running");

  await target.destroy();
  gBrowser.removeCurrentTab();
});

async function toggleAndCheckStates(nodeFront, playState) {
  const animations = await nodeFront.targetFront.getFront("animations");
  const [player] = await animations.getAnimationPlayersForNode(nodeFront);

  if (playState === "paused") {
    await animations.pauseSome([player]);
  } else {
    await animations.playSome([player]);
  }

  info("Getting the AnimationPlayerFront for the test node");
  await player.ready;
  const state = await player.getCurrentState();
  is(
    state.playState,
    playState,
    "The playState of the test node is " + playState
  );
}
