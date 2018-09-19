/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Simple checks for the AnimationsActor

add_task(async function() {
  const {client, walker, animations} = await initAnimationsFrontForUrl(
    "data:text/html;charset=utf-8,<title>test</title><div></div>");

  ok(animations, "The AnimationsFront was created");
  ok(animations.getAnimationPlayersForNode,
     "The getAnimationPlayersForNode method exists");
  ok(animations.pauseSome, "The pauseSome method exists");
  ok(animations.playSome, "The playSome method exists");
  ok(animations.setCurrentTimes, "The setCurrentTimes method exists");
  ok(animations.setPlaybackRates, "The setPlaybackRates method exists");
  ok(animations.setWalkerActor, "The setWalkerActor method exists");

  let didThrow = false;
  try {
    await animations.getAnimationPlayersForNode(null);
  } catch (e) {
    didThrow = true;
  }
  ok(didThrow, "An exception was thrown for a missing NodeActor");

  const invalidNode = await walker.querySelector(walker.rootNode, "title");
  const players = await animations.getAnimationPlayersForNode(invalidNode);
  ok(Array.isArray(players), "An array of players was returned");
  is(players.length, 0, "0 players have been returned for the invalid node");

  await client.close();
  gBrowser.removeCurrentTab();
});
