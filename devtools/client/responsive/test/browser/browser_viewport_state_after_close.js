/* Any copyright is dedicated to the Public Domain.
http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test that after closing RDM the page goes back to its original state

const TEST_URL =
  "data:text/html;charset=utf-8,<style>h1 {width: 200px;} @media (hover:none) { h1 {width: 400px;background: tomato;}</style><h1>Hello</h1>";

add_task(async function () {
  const tab = await addTab(TEST_URL);

  reloadOnTouchChange(false);
  reloadOnUAChange(false);
  await pushPref("devtools.responsive.touchSimulation.enabled", true);

  is(await getH1Width(), 200, "<h1> has expected initial width");

  for (let i = 0; i < 10; i++) {
    info("Open responsive design mode");
    await openRDM(tab);

    await SpecialPowers.spawn(gBrowser.selectedBrowser, [], async function () {
      const mql = content.matchMedia("(hover:none)");
      if (mql.matches) {
        return;
      }
      await new Promise(res =>
        mql.addEventListener("change", res, { once: true })
      );
    });

    is(
      await getH1Width(),
      400,
      "<h1> has expected width when RDM and touch simulation are enabled"
    );

    info("Close responsive design mode");
    await closeRDM(tab);

    is(await getH1Width(), 200, "<h1> has expected width after closing RDM");
  }
});

function getH1Width() {
  return SpecialPowers.spawn(gBrowser.selectedBrowser, [], function () {
    return content.document.querySelector("h1").getBoundingClientRect().width;
  });
}
