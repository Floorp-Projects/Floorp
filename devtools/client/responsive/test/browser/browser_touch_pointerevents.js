/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test that simulating touch only dispatches pointer events from a touch event.

const TEST_URL =
  "data:text/html;charset=utf-8," +
  '<div style="width:100px;height:100px;background-color:red"></div>' +
  "</body>";

addRDMTask(TEST_URL, async function ({ ui }) {
  info("Toggling on touch simulation.");
  reloadOnTouchChange(true);
  await toggleTouchSimulation(ui);

  await testPointerEvents(ui);

  info("Toggling off touch simulation.");
  await toggleTouchSimulation(ui);
  reloadOnTouchChange(false);
});

async function testPointerEvents(ui) {
  info("Test that pointer events are from touch events");
  await SpecialPowers.spawn(ui.getViewportBrowser(), [], async function () {
    const div = content.document.querySelector("div");

    div.addEventListener("pointermove", () => {
      div.style["background-color"] = "green"; //rgb(0,128,0)
    });
    div.addEventListener("pointerdown", e => {
      ok(e.pointerType === "touch", "Got pointer event from a touch event.");
    });

    info("Check that the pointerdown event is from a touch event.");
    const pointerDownPromise = ContentTaskUtils.waitForEvent(
      div,
      "pointerdown"
    );

    await EventUtils.synthesizeMouseAtCenter(
      div,
      { type: "mousedown", isSynthesized: false },
      content
    );
    await pointerDownPromise;
    await EventUtils.synthesizeMouseAtCenter(
      div,
      { type: "mouseup", isSynthesized: false },
      content
    );

    info(
      "Check that a pointermove event was never dispatched from the mousemove event"
    );
    await EventUtils.synthesizeMouseAtCenter(
      div,
      { type: "mousemove", isSynthesized: false },
      content
    );

    const win = content.document.defaultView;
    const bg = win.getComputedStyle(div).getPropertyValue("background-color");

    is(
      bg,
      "rgb(255, 0, 0)",
      `div's background color should still be red: rgb(255, 0, 0): got ${bg}`
    );
  });
}
