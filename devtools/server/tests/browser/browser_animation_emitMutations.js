/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test that the AnimationsActor emits events about changed animations on a
// node after getAnimationPlayersForNode was called on that node.

add_task(function* () {
  let {client, walker, animations} =
    yield initAnimationsFrontForUrl(MAIN_DOMAIN + "animation.html");

  info("Retrieve a non-animated node");
  let node = yield walker.querySelector(walker.rootNode, ".not-animated");

  info("Retrieve the animation player for the node");
  let players = yield animations.getAnimationPlayersForNode(node);
  is(players.length, 0, "The node has no animation players");

  info("Listen for new animations");
  let onMutations = once(animations, "mutations");

  info("Add a couple of animation on the node");
  yield node.modifyAttributes([
    {attributeName: "class", newValue: "multiple-animations"}
  ]);
  let changes = yield onMutations;

  ok(true, "The mutations event was emitted");
  is(changes.length, 2, "There are 2 changes in the mutation event");
  ok(changes.every(({type}) => type === "added"), "Both changes are additions");

  let names = changes.map(c => c.player.initialState.name).sort();
  is(names[0], "glow", "The animation 'glow' was added");
  is(names[1], "move", "The animation 'move' was added");

  info("Store the 2 new players for comparing later");
  let p1 = changes[0].player;
  let p2 = changes[1].player;

  info("Listen for removed animations");
  onMutations = once(animations, "mutations");

  info("Remove the animation css class on the node");
  yield node.modifyAttributes([
    {attributeName: "class", newValue: "not-animated"}
  ]);

  changes = yield onMutations;

  ok(true, "The mutations event was emitted");
  is(changes.length, 2, "There are 2 changes in the mutation event");
  ok(changes.every(({type}) => type === "removed"), "Both are removals");
  ok(changes[0].player === p1 || changes[0].player === p2,
    "The first removed player was one of the previously added players");
  ok(changes[1].player === p1 || changes[1].player === p2,
    "The second removed player was one of the previously added players");

  yield closeDebuggerClient(client);
  gBrowser.removeCurrentTab();
});
