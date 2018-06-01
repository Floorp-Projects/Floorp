/* vim: set ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

requestLongerTimeout(2);

// Test that the main toggle button actually toggles animations.
// This test doesn't need to be extra careful about checking that *all*
// animations have been paused (including inside iframes) because there's an
// actor test in /devtools/server/tests/browser/ that does this.

add_task(async function() {
  await addTab(URL_ROOT + "doc_simple_animation.html");
  const {panel} = await openAnimationInspector();

  info("Click the toggle button");
  await panel.toggleAll();
  await checkState("paused");

  info("Click again the toggle button");
  await panel.toggleAll();
  await checkState("running");
});

async function checkState(state) {
  for (const selector of [".animated", ".multi", ".long"]) {
    const playState = await getAnimationPlayerState(selector);
    is(playState, state, "The animation on node " + selector + " is " + state);
  }
}
