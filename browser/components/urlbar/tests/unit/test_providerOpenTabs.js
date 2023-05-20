/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

add_task(async function test_openTabs() {
  const userContextId = 5;
  const url = "http://foo.mozilla.org/";
  UrlbarProviderOpenTabs.registerOpenTab(url, userContextId, false);
  UrlbarProviderOpenTabs.registerOpenTab(url, userContextId, false);
  Assert.equal(
    UrlbarProviderOpenTabs._openTabs.get(userContextId).length,
    2,
    "Found all the expected tabs"
  );
  UrlbarProviderOpenTabs.unregisterOpenTab(url, userContextId, false);
  Assert.equal(
    UrlbarProviderOpenTabs._openTabs.get(userContextId).length,
    1,
    "Found all the expected tabs"
  );

  let context = createContext();
  let matchCount = 0;
  let callback = function (provider, match) {
    matchCount++;
    Assert.ok(
      provider instanceof UrlbarProviderOpenTabs,
      "Got the expected provider"
    );
    Assert.equal(
      match.type,
      UrlbarUtils.RESULT_TYPE.TAB_SWITCH,
      "Got the expected result type"
    );
    Assert.equal(match.payload.url, url, "Got the expected url");
    Assert.equal(match.payload.title, undefined, "Got the expected title");
  };

  let provider = new UrlbarProviderOpenTabs();
  await provider.startQuery(context, callback);
  Assert.equal(matchCount, 1, "Found the expected number of matches");
  // Sanity check that this doesn't throw.
  provider.cancelQuery(context);
});
