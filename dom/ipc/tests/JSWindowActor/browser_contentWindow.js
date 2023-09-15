/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */
"use strict";

const CONTENT_WINDOW_URL = "https://example.com/";

declTest("contentWindow null when inner window inactive", {
  matches: [CONTENT_WINDOW_URL + "*"],
  url: CONTENT_WINDOW_URL + "?1",

  async test(browser) {
    // If Fission is disabled, the pref is no-op.
    await SpecialPowers.pushPrefEnv({
      set: [["fission.bfcacheInParent", true]],
    });

    let parent = browser.browsingContext.currentWindowGlobal;
    let actorParent = parent.getActor("TestWindow");

    await actorParent.sendQuery("storeActor");

    let url = CONTENT_WINDOW_URL + "?2";
    let loaded = BrowserTestUtils.browserLoaded(browser, false, url);
    BrowserTestUtils.startLoadingURIString(browser, url);
    await loaded;

    let result = await actorParent.sendQuery("checkActor");

    is(result.status, "success", "Should succeed when bfcache is enabled");
    ok(
      result.valueIsNull,
      "Should get a null contentWindow when inner window is inactive"
    );
  },
});
