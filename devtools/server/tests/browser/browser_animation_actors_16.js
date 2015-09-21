/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Check that the AnimationsActor can retrieve all animations inside a node's
// subtree (but not going into iframes).

const {AnimationsFront} = require("devtools/server/actors/animation");
const {InspectorFront} = require("devtools/server/actors/inspector");

const URL = MAIN_DOMAIN + "animation.html";

add_task(function*() {
  info("Creating a test document with 2 iframes containing animated nodes");
  yield addTab("data:text/html;charset=utf-8," +
               "<iframe id='iframe' src='" + URL + "'></iframe>");

  initDebuggerServer();
  let client = new DebuggerClient(DebuggerServer.connectPipe());
  let form = yield connectDebuggerClient(client);
  let inspector = InspectorFront(client, form);
  let walker = yield inspector.getWalker();
  let front = AnimationsFront(client, form);

  info("Try retrieving all animations from the root doc's <body> node");
  let rootBody = yield walker.querySelector(walker.rootNode, "body");
  let players = yield front.getAnimationPlayersForNode(rootBody);
  is(players.length, 0, "The node has no animation players");

  info("Retrieve all animations from the iframe's <body> node");
  let iframe = yield walker.querySelector(walker.rootNode, "#iframe");
  let {nodes} = yield walker.children(iframe);
  let frameBody = yield walker.querySelector(nodes[0], "body");
  players = yield front.getAnimationPlayersForNode(frameBody);

  // Testing for a hard-coded number of animations here would intermittently
  // fail depending on how fast or slow the test is (indeed, the test page
  // contains short transitions, and delayed animations). So just make sure we
  // at least have the infinitely running animations.
  ok(players.length >= 4, "All subtree animations were retrieved");

  yield closeDebuggerClient(client);
  gBrowser.removeCurrentTab();
});
