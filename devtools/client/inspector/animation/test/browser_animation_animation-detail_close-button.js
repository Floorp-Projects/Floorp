/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test that whether close button in header of animation detail works.

add_task(async function () {
  await addTab(URL_ROOT + "doc_multi_timings.html");
  const { animationInspector, panel } = await openAnimationInspector();

  info("Checking close button in header of animation detail");
  await clickOnAnimation(animationInspector, panel, 0);
  const detailEl = panel.querySelector("#animation-container .controlled");
  const win = panel.ownerGlobal;
  isnot(win.getComputedStyle(detailEl).display, "none",
    "detailEl should be visibled before clicking close button");
  clickOnDetailCloseButton(panel);
  is(win.getComputedStyle(detailEl).display, "none",
    "detailEl should be unvisibled after clicking close button");
});
