/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

add_task(async function maxTouchPoints() {
  await new Promise(resolve => {
    SpecialPowers.pushPrefEnv(
      {
        set: [["dom.maxtouchpoints.testing.value", 5]],
      },
      resolve
    );
  });

  let tab = await BrowserTestUtils.openNewForegroundTab(
    gBrowser,
    "data:text/html,Test page for navigator.maxTouchPoints"
  );
  await SpecialPowers.spawn(tab.linkedBrowser, [], function () {
    is(content.navigator.maxTouchPoints, 5, "Should have touch points.");
  });

  BrowserTestUtils.removeTab(tab);
});
