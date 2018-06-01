/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test that the AnimationsActor emits events about changed animations on a
// node after getAnimationPlayersForNode was called on that node.

add_task(async function() {
  const {client, walker, animations} =
    await initAnimationsFrontForUrl(MAIN_DOMAIN + "animation.html");

  info("Retrieve a non-animated node");
  const node = await walker.querySelector(walker.rootNode, ".not-animated");

  info("Retrieve the animation player for the node");
  const players = await animations.getAnimationPlayersForNode(node);
  is(players.length, 0, "The node has no animation players");

  info("Listen for new animations");
  let onMutations = once(animations, "mutations");

  info("Add a couple of animation on the node");
  await node.modifyAttributes([
    {attributeName: "class", newValue: "multiple-animations"}
  ]);
  let changes = await onMutations;

  ok(true, "The mutations event was emitted");
  is(changes.length, 2, "There are 2 changes in the mutation event");
  ok(changes.every(({type}) => type === "added"), "Both changes are additions");

  const names = changes.map(c => c.player.initialState.name).sort();
  is(names[0], "glow", "The animation 'glow' was added");
  is(names[1], "move", "The animation 'move' was added");

  info("Store the 2 new players for comparing later");
  const p1 = changes[0].player;
  const p2 = changes[1].player;

  info("Listen for removed animations");
  onMutations = once(animations, "mutations");

  info("Remove the animation css class on the node");
  await node.modifyAttributes([
    {attributeName: "class", newValue: "not-animated"}
  ]);

  changes = await onMutations;

  ok(true, "The mutations event was emitted");
  is(changes.length, 2, "There are 2 changes in the mutation event");
  ok(changes.every(({type}) => type === "removed"), "Both are removals");
  ok(changes[0].player === p1 || changes[0].player === p2,
    "The first removed player was one of the previously added players");
  ok(changes[1].player === p1 || changes[1].player === p2,
    "The second removed player was one of the previously added players");

  await client.close();
  gBrowser.removeCurrentTab();
});
