/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Check that the AnimationPlayerActor exposes a getProperties method that
// returns the list of animated properties in the animation.

const URL = MAIN_DOMAIN + "animation.html";

add_task(async function() {
  const {client, walker, animations} = await initAnimationsFrontForUrl(URL);

  info("Get the test node and its animation front");
  const node = await walker.querySelector(walker.rootNode, ".simple-animation");
  const [player] = await animations.getAnimationPlayersForNode(node);

  ok(player.getProperties, "The front has the getProperties method");

  const properties = await player.getProperties();
  is(properties.length, 1, "The correct number of properties was retrieved");

  const propertyObject = properties[0];
  is(propertyObject.name, "transform", "Property 0 is transform");

  is(propertyObject.values.length, 2,
    "The correct number of property values was retrieved");

  // Note that we don't really test the content of the frame object here on
  // purpose. This object comes straight out of the web animations API
  // unmodified.

  await client.close();
  gBrowser.removeCurrentTab();
});
