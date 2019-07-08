/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test that animations with an overflowed delay and end delay are not displayed.

add_task(async function() {
  await addTab(URL_ROOT + "doc_overflowed_delay_end_delay.html");
  const { panel } = await openAnimationInspector();

  info("Check the number of animation item");
  const animationItemEls = panel.querySelectorAll(
    ".animation-list .animation-item"
  );
  is(
    animationItemEls.length,
    1,
    "The number of animations displayed should be 1"
  );

  info("Check the id of animation displayed");
  const animationNameEl = animationItemEls[0].querySelector(".animation-name");
  is(
    animationNameEl.textContent,
    "big-iteration-start",
    "The animation name should be 'big-iteration-start'"
  );
});
