/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */
"use strict";
SimpleTest.requestFlakyTimeout(
  "The test needs to let objects die asynchronously."
);

add_task(async function test_accessing_shistory() {
  let tab = await BrowserTestUtils.openNewForegroundTab(
    gBrowser,
    "about:preferences"
  );
  let sh = tab.linkedBrowser.browsingContext.sessionHistory;
  ok(sh, "Should have SessionHistory object");
  gBrowser.removeTab(tab);
  tab = null;
  for (let i = 0; i < 5; ++i) {
    SpecialPowers.Services.obs.notifyObservers(
      null,
      "memory-pressure",
      "heap-minimize"
    );
    SpecialPowers.DOMWindowUtils.garbageCollect();
    await new Promise(function (r) {
      // eslint-disable-next-line mozilla/no-arbitrary-setTimeout
      setTimeout(r, 50);
    });
  }

  try {
    sh.reloadCurrentEntry();
  } catch (ex) {}
  ok(true, "This test shouldn't crash.");
});
