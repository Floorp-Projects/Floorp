/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Check that the AnimationPlayerActor exposes a getFrames method that returns
// the list of keyframes in the animation.

add_task(async function() {
  const {client, walker, animations} =
    await initAnimationsFrontForUrl(MAIN_DOMAIN + "animation.html");

  info("Get the test node and its animation front");
  const node = await walker.querySelector(walker.rootNode, ".simple-animation");
  const [player] = await animations.getAnimationPlayersForNode(node);

  ok(player.getFrames, "The front has the getFrames method");

  const frames = await player.getFrames();
  is(frames.length, 2, "The correct number of keyframes was retrieved");
  ok(frames[0].transform, "Frame 0 has the transform property");
  ok(frames[1].transform, "Frame 1 has the transform property");
  // Note that we don't really test the content of the frame object here on
  // purpose. This object comes straight out of the web animations API
  // unmodified.

  await client.close();
  gBrowser.removeCurrentTab();
});
