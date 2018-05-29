/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test whether the created time of animation unchanged even if change node.

add_task(async function() {
  await addTab(URL_ROOT + "doc_custom_playback_rate.html");
  const { animationInspector, inspector } = await openAnimationInspector();

  info("Check both the created time of animation are same");
  const baseCreatedTime = animationInspector.state.animations[0].state.createdTime;
  is(animationInspector.state.animations[1].state.createdTime, baseCreatedTime,
    "Both created time of animations should be same");

  info("Check created time after selecting '.div1'");
  await selectNodeAndWaitForAnimations(".div1", inspector);
  is(animationInspector.state.animations[0].state.createdTime, baseCreatedTime,
    "The created time of animation on element of .div1 should unchanged");

  info("Check created time after selecting '.div2'");
  await selectNodeAndWaitForAnimations(".div2", inspector);
  is(animationInspector.state.animations[0].state.createdTime, baseCreatedTime,
     "The created time of animation on element of .div2 should unchanged");

  info("Check created time after selecting 'body' again");
  await selectNodeAndWaitForAnimations("body", inspector);
  is(animationInspector.state.animations[0].state.createdTime, baseCreatedTime,
    "The created time of animation[0] should unchanged");
  is(animationInspector.state.animations[1].state.createdTime, baseCreatedTime,
    "The created time of animation[1] should unchanged");
});
