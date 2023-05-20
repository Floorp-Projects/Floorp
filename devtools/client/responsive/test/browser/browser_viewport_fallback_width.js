/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test that the viewport's ICB width will use the simulated screen width
// if the simulated width is larger than the desktop viewport width default
// (980px).

// The HTML below sets up the test such that the "inner" div is aligned to the end
// (right-side) of the viewport. By doing this, it makes it easier to have our test
// target an element whose bounds are outside of the desktop viewport width default
// for device screens greater than 980px.
const TEST_URL =
  `data:text/html;charset=utf-8,` +
  `<div id="outer" style="display: grid; justify-items: end; font-size: 64px">
    <div id="inner">Click me!</div>
  </div>`;

addRDMTask(TEST_URL, async function ({ ui, manager }) {
  info("Toggling on touch simulation.");
  reloadOnTouchChange(true);
  await toggleTouchSimulation(ui);
  // It's important we set a viewport width larger than 980px for this test to be correct.
  // So let's choose viewport width: 1280x600
  await setViewportSizeAndAwaitReflow(ui, manager, 1280, 600);

  await testICBWidth(ui);

  info("Toggling off touch simulation.");
  await toggleTouchSimulation(ui);
  reloadOnTouchChange(false);
});

async function testICBWidth(ui) {
  await SpecialPowers.spawn(ui.getViewportBrowser(), [], async function () {
    const innerDiv = content.document.getElementById("inner");

    innerDiv.addEventListener("click", () => {
      innerDiv.style.color = "green"; //rgb(0,128,0)
    });

    info("Check that touch point (via click) registers on inner div.");
    const mousedown = ContentTaskUtils.waitForEvent(innerDiv, "click");
    await EventUtils.synthesizeClick(innerDiv);
    await mousedown;

    const win = content.document.defaultView;
    const bg = win.getComputedStyle(innerDiv).getPropertyValue("color");

    is(bg, "rgb(0, 128, 0)", "inner div's background color changed to green.");
  });
}
