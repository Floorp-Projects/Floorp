/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test that whether animations detail could be displayed if there is selected animation.

add_task(async function () {
  await addTab(URL_ROOT + "doc_custom_playback_rate.html");
  const { animationInspector, inspector, panel } =
    await openAnimationInspector();

  info("Checking animation detail visibility if animation was unselected");
  const detailEl = panel.querySelector("#animation-container .controlled");
  ok(detailEl, "The detail pane should be in the DOM");
  await assertDisplayStyle(detailEl, true, "detailEl should be unvisibled");

  info(
    "Checking animation detail visibility if animation was selected by click"
  );
  await clickOnAnimation(animationInspector, panel, 0);
  await assertDisplayStyle(detailEl, false, "detailEl should be visibled");

  info(
    "Checking animation detail visibility when choose node which has animations"
  );
  await selectNode("html", inspector);
  await assertDisplayStyle(
    detailEl,
    true,
    "detailEl should be unvisibled after choose html node"
  );

  info(
    "Checking animation detail visibility when choose node which has an animation"
  );
  await selectNode("div", inspector);
  await assertDisplayStyle(
    detailEl,
    false,
    "detailEl should be visibled after choose .cssanimation-normal node"
  );
});

async function assertDisplayStyle(detailEl, isNoneExpected, description) {
  const win = detailEl.ownerGlobal;
  await waitUntil(() => {
    const isNone = win.getComputedStyle(detailEl).display === "none";
    return isNone === isNoneExpected;
  });
  ok(true, description);
}
