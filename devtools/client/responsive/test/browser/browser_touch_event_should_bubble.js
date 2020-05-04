/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test that simulated touch events bubble.

const TEST_URL = `${URL_ROOT}touch_event_bubbles.html`;

addRDMTask(
  TEST_URL,
  async function({ ui }) {
    info("Toggling on touch simulation.");
    reloadOnTouchChange(true);
    await toggleTouchSimulation(ui);

    info("Test that touch event bubbles.");
    await SpecialPowers.spawn(ui.getViewportBrowser(), [], async function() {
      const outerDiv = content.document.getElementById("outer");
      const span = content.document.querySelector("span");

      outerDiv.addEventListener("touchstart", () => {
        span.style["background-color"] = "green"; // rgb(0, 128, 0)
      });

      const touchStartPromise = ContentTaskUtils.waitForEvent(
        span,
        "touchstart"
      );
      await EventUtils.synthesizeMouseAtCenter(
        span,
        { type: "mousedown", isSynthesized: false },
        content
      );
      await touchStartPromise;

      const win = content.document.defaultView;
      const bg = win
        .getComputedStyle(span)
        .getPropertyValue("background-color");

      is(
        bg,
        "rgb(0, 128, 0)",
        `span's background color should be rgb(0, 128, 0): got ${bg}`
      );

      await EventUtils.synthesizeMouseAtCenter(
        span,
        { type: "mouseup", isSynthesized: false },
        content
      );
    });

    info("Toggling off touch simulation.");
    await toggleTouchSimulation(ui);
    reloadOnTouchChange(false);
  },
  { usingBrowserUI: true }
);
