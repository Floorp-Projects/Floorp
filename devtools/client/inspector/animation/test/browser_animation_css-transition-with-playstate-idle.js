/* Any copyright is dedicated to the Public Domain.
http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test that animation inspector does not fail when rendering an animation that
// transitions from the playState "idle".

const PAGE_URL = `data:text/html;charset=utf-8,
<!DOCTYPE html>
<html>
<head>
  <style type="text/css">
    div {
      opacity: 0;
      transition-duration: 5000ms;
      transition-property: opacity;
    }

    div.visible {
      opacity: 1;
    }
  </style>
</head>
<body>
  <div>test</div>
</body>
</html>`;

add_task(async function() {
  const tab = await addTab(PAGE_URL);
  const { animationInspector, panel } = await openAnimationInspector();

  info("Toggle the visible class to start the animation");
  await toggleVisibleClass(tab);

  info("Wait until the scrubber is displayed");
  await waitUntil(() => panel.querySelector(".current-time-scrubber"));

  const scrubberEl = panel.querySelector(".current-time-scrubber");

  info("Wait until animations are paused");
  await waitUntilAnimationsPaused(animationInspector);

  // Check the initial position of the scrubber to detect the animation.
  const scrubberX = scrubberEl.getBoundingClientRect().x;

  info("Toggle the visible class to start the animation");
  await toggleVisibleClass(tab);

  info("scrubberX after: " + scrubberEl.getBoundingClientRect().x);

  info("Wait until the scrubber starts moving");
  await waitUntil(() => scrubberEl.getBoundingClientRect().x != scrubberX);

  info("Wait until animations are paused");
  await waitUntilAnimationsPaused(animationInspector);

  // Query the scrubber element again to check that the UI is still rendered.
  ok(!!panel.querySelector(".current-time-scrubber"),
    "The scrubber element is still rendered in the animation inspector panel");

  info("Wait for the keyframes graph to be updated before ending the test.");
  await waitForAnimationDetail(animationInspector);
});

/**
 * Local helper to toggle the "visible" class on the element with a transition defined.
 */
async function toggleVisibleClass(tab) {
  await ContentTask.spawn(tab.linkedBrowser, {}, async function() {
    let win = content.wrappedJSObject;
    win.document.querySelector("div").classList.toggle("visible");
  });
}

async function waitUntilAnimationsPaused(animationInspector) {
  await waitUntil(() => {
    const animations = animationInspector.state.animations;
    return animations.every(animation => {
      const state = animation.state.playState;
      return state === "paused" || state === "finished";
    });
  });
}
