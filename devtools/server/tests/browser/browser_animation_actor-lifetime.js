/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test for Bug 1247243

add_task(async function setup() {
  info("Setting up inspector and animation actors.");
  const { animations, walker } =
    await initAnimationsFrontForUrl(MAIN_DOMAIN + "animation-data.html");

  info("Testing animated node actor");
  const animatedNodeActor = await walker.querySelector(walker.rootNode,
    ".animated");
  await animations.getAnimationPlayersForNode(animatedNodeActor);

  await assertNumberOfAnimationActors(1, "AnimationActor have 1 AnimationPlayerActors");

  info("Testing AnimationPlayerActors release");
  const stillNodeActor = await walker.querySelector(walker.rootNode,
    ".still");
  await animations.getAnimationPlayersForNode(stillNodeActor);
  await assertNumberOfAnimationActors(0,
    "AnimationActor does not have any AnimationPlayerActors anymore");

  info("Testing multi animated node actor");
  const multiNodeActor = await walker.querySelector(walker.rootNode,
    ".multi");
  await animations.getAnimationPlayersForNode(multiNodeActor);
  await assertNumberOfAnimationActors(2,
    "AnimationActor has now 2 AnimationPlayerActors");

  info("Testing single animated node actor");
  await animations.getAnimationPlayersForNode(animatedNodeActor);
  await assertNumberOfAnimationActors(1,
    "AnimationActor has only one AnimationPlayerActors");

  info("Testing AnimationPlayerActors release again");
  await animations.getAnimationPlayersForNode(stillNodeActor);
  await assertNumberOfAnimationActors(0,
    "AnimationActor does not have any AnimationPlayerActors anymore");

  async function assertNumberOfAnimationActors(expected, message) {
    const actors = await ContentTask.spawn(
      gBrowser.selectedBrowser,
      [animations.actorID],
      function(actorID) {
        const { require } = ChromeUtils.import("resource://devtools/shared/Loader.jsm", {});
        const { DebuggerServer } = require("devtools/server/main");
        // Convert actorID to current compartment string otherwise
        // searchAllConnectionsForActor is confused and won't find the actor.
        actorID = String(actorID);
        const animationActors = DebuggerServer
          .searchAllConnectionsForActor(actorID);
        if (!animationActors) {
          return 0;
        }
        return animationActors.actors.length;
      }
    );
    is(actors, expected, message);
  }
});
