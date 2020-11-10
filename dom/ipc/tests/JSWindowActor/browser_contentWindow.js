/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */
"use strict";

declTest("contentWindow null when inner window inactive", {
  matches: [TEST_URL + "*"],
  url: TEST_URL + "?1",

  async test(browser) {
    {
      let parent = browser.browsingContext.currentWindowGlobal;
      let actorParent = parent.getActor("TestWindow");

      await actorParent.sendQuery("storeActor");
    }

    {
      let url = TEST_URL + "?2";
      let loaded = BrowserTestUtils.browserLoaded(browser, false, url);
      await BrowserTestUtils.loadURI(browser, url);
      await loaded;
    }

    let parent = browser.browsingContext.currentWindowGlobal;
    let actorParent = parent.getActor("TestWindow");

    let result = await actorParent.sendQuery("checkActor");
    if (SpecialPowers.useRemoteSubframes) {
      is(
        result.status,
        "error",
        "Should get an error when bfcache is disabled for Fission"
      );
      is(
        result.errorType,
        "InvalidStateError",
        "Should get an InvalidStateError without bfcache"
      );
    } else {
      is(result.status, "success", "Should succeed when bfcache is enabled");
      ok(
        result.valueIsNull,
        "Should get a null contentWindow when inner window is inactive"
      );
    }
  },
});
