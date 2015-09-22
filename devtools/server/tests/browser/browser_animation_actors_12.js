/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Check that a player's playbackRate can be changed.

const {AnimationsFront} = require("devtools/server/actors/animation");
const {InspectorFront} = require("devtools/server/actors/inspector");

add_task(function*() {
  yield addTab(MAIN_DOMAIN + "animation.html");

  initDebuggerServer();
  let client = new DebuggerClient(DebuggerServer.connectPipe());
  let form = yield connectDebuggerClient(client);
  let inspector = InspectorFront(client, form);
  let walker = yield inspector.getWalker();
  let animations = AnimationsFront(client, form);

  info("Retrieve an animated node");
  let node = yield walker.querySelector(walker.rootNode, ".simple-animation");

  info("Retrieve the animation player for the node");
  let [player] = yield animations.getAnimationPlayersForNode(node);

  ok(player.setPlaybackRate, "Player has the setPlaybackRate method");

  info("Change the rate to 10");
  yield player.setPlaybackRate(10);

  info("Query the state again");
  let state = yield player.getCurrentState();
  is(state.playbackRate, 10, "The playbackRate was updated");

  info("Change the rate back to 1");
  yield player.setPlaybackRate(1);

  info("Query the state again");
  state = yield player.getCurrentState();
  is(state.playbackRate, 1, "The playbackRate was changed back");

  yield closeDebuggerClient(client);
  gBrowser.removeCurrentTab();
});
