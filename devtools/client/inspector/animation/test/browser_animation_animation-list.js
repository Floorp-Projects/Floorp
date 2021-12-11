/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test that whether animations ui could be displayed

add_task(async function() {
  await addTab(URL_ROOT + "doc_simple_animation.html");
  await removeAnimatedElementsExcept([".animated", ".long"]);
  const {
    animationInspector,
    inspector,
    panel,
  } = await openAnimationInspector();

  info("Checking animation list and items existence");
  ok(
    panel.querySelector(".animation-list"),
    "The animation-list is in the DOM"
  );
  is(
    panel.querySelectorAll(".animation-list .animation-item").length,
    animationInspector.state.animations.length,
    "The number of animations displayed matches the number of animations"
  );

  info(
    "Checking list and items existence after select a element which has an animation"
  );
  await selectNode(".animated", inspector);
  await waitUntil(
    () => panel.querySelectorAll(".animation-list .animation-item").length === 1
  );
  ok(
    true,
    "The number of animations displayed should be 1 for .animated element"
  );
});
