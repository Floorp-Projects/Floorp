/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test of the content of tick label on timeline header
// with the animation which has infinity duration.

add_task(async function() {
  await addTab(URL_ROOT + "doc_infinity_duration.html");
  const { inspector, panel } = await openAnimationInspector();

  info("Check the tick label content with limited duration animation");
  isnot(
    panel.querySelector(".animation-list-container .tick-label:last-child").textContent,
    "\u221E",
    "The content should not be \u221E"
  );

  info("Check the tick label content with infinity duration animation only");
  await selectNodeAndWaitForAnimations(".infinity", inspector);
  is(
    panel.querySelector(".animation-list-container .tick-label:last-child").textContent,
    "\u221E",
    "The content should be \u221E"
  );
});
