/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

add_task(async function test_openTabs() {
  const userContextId1 = 3;
  const userContextId2 = 5;
  const url = "http://foo.mozilla.org/";
  const url2 = "http://foo2.mozilla.org/";
  UrlbarProviderOpenTabs.registerOpenTab(url, userContextId1, false);
  UrlbarProviderOpenTabs.registerOpenTab(url, userContextId1, false);
  UrlbarProviderOpenTabs.registerOpenTab(url2, userContextId1, false);
  UrlbarProviderOpenTabs.registerOpenTab(url, userContextId2, false);
  Assert.deepEqual(
    [url, url2],
    UrlbarProviderOpenTabs.getOpenTabs(userContextId1),
    "Found all the expected tabs"
  );
  Assert.deepEqual(
    [url],
    UrlbarProviderOpenTabs.getOpenTabs(userContextId2),
    "Found all the expected tabs"
  );
  await PlacesUtils.promiseLargeCacheDBConnection();
  await UrlbarProviderOpenTabs.promiseDBPopulated;
  Assert.deepEqual(
    [
      { url, userContextId: userContextId1, count: 2 },
      { url: url2, userContextId: userContextId1, count: 1 },
      { url, userContextId: userContextId2, count: 1 },
    ],
    await UrlbarProviderOpenTabs.getDatabaseRegisteredOpenTabsForTests(),
    "Found all the expected tabs"
  );

  await UrlbarProviderOpenTabs.unregisterOpenTab(url2, userContextId1, false);
  Assert.deepEqual(
    [url],
    UrlbarProviderOpenTabs.getOpenTabs(userContextId1),
    "Found all the expected tabs"
  );
  await UrlbarProviderOpenTabs.unregisterOpenTab(url, userContextId1, false);
  Assert.deepEqual(
    [url],
    UrlbarProviderOpenTabs.getOpenTabs(userContextId1),
    "Found all the expected tabs"
  );
  Assert.deepEqual(
    [
      { url, userContextId: userContextId1, count: 1 },
      { url, userContextId: userContextId2, count: 1 },
    ],
    await UrlbarProviderOpenTabs.getDatabaseRegisteredOpenTabsForTests(),
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
  Assert.equal(matchCount, 2, "Found the expected number of matches");
  // Sanity check that this doesn't throw.
  provider.cancelQuery(context);
});

add_task(async function test_openTabs_mixedtype_input() {
  // Passing the userContextId as a string, rather than a number, is a fairly
  // common mistake, check the API handles both properly.
  const url = "http://someurl.mozilla.org/";
  Assert.deepEqual(
    [],
    UrlbarProviderOpenTabs.getOpenTabs(1),
    "Found all the expected tabs"
  );
  Assert.deepEqual(
    [],
    UrlbarProviderOpenTabs.getOpenTabs(2),
    "Found all the expected tabs"
  );
  UrlbarProviderOpenTabs.registerOpenTab(url, 1, false);
  UrlbarProviderOpenTabs.registerOpenTab(url, "2", false);
  Assert.deepEqual(
    [url],
    UrlbarProviderOpenTabs.getOpenTabs(1),
    "Found all the expected tabs"
  );
  Assert.deepEqual(
    [url],
    UrlbarProviderOpenTabs.getOpenTabs(2),
    "Found all the expected tabs"
  );
  Assert.deepEqual(
    UrlbarProviderOpenTabs.getOpenTabs(1),
    UrlbarProviderOpenTabs.getOpenTabs("1"),
    "Also check getOpenTabs adapts to the argument type"
  );
  UrlbarProviderOpenTabs.unregisterOpenTab(url, "1", false);
  UrlbarProviderOpenTabs.unregisterOpenTab(url, 2, false);
  Assert.deepEqual(
    [],
    UrlbarProviderOpenTabs.getOpenTabs(1),
    "Found all the expected tabs"
  );
  Assert.deepEqual(
    [],
    UrlbarProviderOpenTabs.getOpenTabs(2),
    "Found all the expected tabs"
  );
});
