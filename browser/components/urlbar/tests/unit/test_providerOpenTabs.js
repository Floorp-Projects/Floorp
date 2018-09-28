/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

add_task(async function test_openTabs() {
  const userContextId = 5;
  const url = "http://foo.mozilla.org/";
  UrlbarProviderOpenTabs.registerOpenTab(url, userContextId);
  UrlbarProviderOpenTabs.registerOpenTab(url, userContextId);
  Assert.equal(UrlbarProviderOpenTabs.openTabs.get(userContextId).length, 2,
               "Found all the expected tabs");
  UrlbarProviderOpenTabs.unregisterOpenTab(url, userContextId);
  Assert.equal(UrlbarProviderOpenTabs.openTabs.get(userContextId).length, 1,
               "Found all the expected tabs");

  let context = createContext();
  let matchCount = 0;
  let callback = function(provider, match) {
    matchCount++;
    Assert.equal(provider, UrlbarProviderOpenTabs, "Got the expected provider");
    Assert.equal(match.type, UrlbarUtils.MATCH_TYPE.TAB_SWITCH,
                 "Got the expected match type");
    Assert.equal(match.url, url, "Got the expected url");
    Assert.equal(match.title, "", "Got the expected title");
  };

  await UrlbarProviderOpenTabs.startQuery(context, callback);
  Assert.equal(matchCount, 1, "Found the expected number of matches");
  // Sanity check that this doesn't throw.
  UrlbarProviderOpenTabs.cancelQuery(context);
  Assert.equal(UrlbarProviderOpenTabs.queries.size, 0,
    "All the queries have been removed");
});
