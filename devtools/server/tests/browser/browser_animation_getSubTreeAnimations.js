/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Check that the AnimationsActor can retrieve all animations inside a node's
// subtree (but not going into iframes).

const URL = MAIN_DOMAIN + "animation.html";

// Import inspector's shared head.
Services.scriptloader.loadSubScript(
  "chrome://mochitests/content/browser/devtools/client/inspector/test/shared-head.js",
  this
);

add_task(async function () {
  info("Creating a test document with 2 iframes containing animated nodes");

  const { inspector, target, walker, animations } =
    await initAnimationsFrontForUrl(
      "data:text/html;charset=utf-8," +
        "<iframe id='iframe' src='" +
        URL +
        "'></iframe>"
    );

  info("Try retrieving all animations from the root doc's <body> node");
  const rootBody = await walker.querySelector(walker.rootNode, "body");
  let players = await animations.getAnimationPlayersForNode(rootBody);
  is(players.length, 0, "The node has no animation players");

  info("Retrieve all animations from the iframe's <body> node");
  const frameBody = await getNodeFrontInFrames(["#iframe", "body"], inspector);
  const animationsForFrame = await frameBody.targetFront.getFront("animations");
  players = await animationsForFrame.getAnimationPlayersForNode(frameBody);

  // Testing for a hard-coded number of animations here would intermittently
  // fail depending on how fast or slow the test is (indeed, the test page
  // contains short transitions, and delayed animations). So just make sure we
  // at least have the infinitely running animations.
  ok(players.length >= 4, "All subtree animations were retrieved");

  await target.destroy();
  gBrowser.removeCurrentTab();
});
