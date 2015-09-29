/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Check that the AnimationPlayerActor exposes a getFrames method that returns
// the list of keyframes in the animation.

const {AnimationsFront} = require("devtools/server/actors/animation");
const {InspectorFront} = require("devtools/server/actors/inspector");

const URL = MAIN_DOMAIN + "animation.html";

add_task(function*() {
  yield addTab(MAIN_DOMAIN + "animation.html");

  initDebuggerServer();
  let client = new DebuggerClient(DebuggerServer.connectPipe());
  let form = yield connectDebuggerClient(client);
  let inspector = InspectorFront(client, form);
  let walker = yield inspector.getWalker();
  let front = AnimationsFront(client, form);

  info("Get the test node and its animation front");
  let node = yield walker.querySelector(walker.rootNode, ".simple-animation");
  let [player] = yield front.getAnimationPlayersForNode(node);

  ok(player.getFrames, "The front has the getFrames method");

  let frames = yield player.getFrames();
  is(frames.length, 2, "The correct number of keyframes was retrieved");
  ok(frames[0].transform, "Frame 0 has the transform property");
  ok(frames[1].transform, "Frame 1 has the transform property");
  // Note that we don't really test the content of the frame object here on
  // purpose. This object comes straight out of the web animations API
  // unmodified.

  yield closeDebuggerClient(client);
  gBrowser.removeCurrentTab();
});
