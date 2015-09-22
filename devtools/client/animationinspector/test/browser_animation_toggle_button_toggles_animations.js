/* vim: set ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test that the main toggle button actually toggles animations.
// This test doesn't need to be extra careful about checking that *all*
// animations have been paused (including inside iframes) because there's an
// actor test in /devtools/server/tests/browser/ that does this.

add_task(function*() {
  yield addTab(TEST_URL_ROOT + "doc_simple_animation.html");
  let {panel} = yield openAnimationInspector();

  info("Click the toggle button");
  yield panel.toggleAll();
  yield checkState("paused");

  info("Click again the toggle button");
  yield panel.toggleAll();
  yield checkState("running");
});

function* checkState(state) {
  for (let selector of [".animated", ".multi", ".long"]) {
    let playState = yield getAnimationPlayerState(selector);
    is(playState, state, "The animation on node " + selector + " is " + state);
  }
}
