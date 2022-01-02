const { ASRouterTargeting, QueryCache } = ChromeUtils.import(
  "resource://activity-stream/lib/ASRouterTargeting.jsm"
);
const { AddonTestUtils } = ChromeUtils.import(
  "resource://testing-common/AddonTestUtils.jsm"
);
const { CFRMessageProvider } = ChromeUtils.import(
  "resource://activity-stream/lib/CFRMessageProvider.jsm"
);
ChromeUtils.defineModuleGetter(
  this,
  "ProfileAge",
  "resource://gre/modules/ProfileAge.jsm"
);
ChromeUtils.defineModuleGetter(
  this,
  "AddonManager",
  "resource://gre/modules/AddonManager.jsm"
);
ChromeUtils.defineModuleGetter(
  this,
  "ShellService",
  "resource:///modules/ShellService.jsm"
);
ChromeUtils.defineModuleGetter(
  this,
  "NewTabUtils",
  "resource://gre/modules/NewTabUtils.jsm"
);
ChromeUtils.defineModuleGetter(
  this,
  "PlacesTestUtils",
  "resource://testing-common/PlacesTestUtils.jsm"
);
ChromeUtils.defineModuleGetter(
  this,
  "TelemetryEnvironment",
  "resource://gre/modules/TelemetryEnvironment.jsm"
);
ChromeUtils.defineModuleGetter(
  this,
  "AppConstants",
  "resource://gre/modules/AppConstants.jsm"
);
ChromeUtils.defineModuleGetter(
  this,
  "Region",
  "resource://gre/modules/Region.jsm"
);
ChromeUtils.defineModuleGetter(
  this,
  "HomePage",
  "resource:///modules/HomePage.jsm"
);
ChromeUtils.defineModuleGetter(
  this,
  "AboutNewTab",
  "resource:///modules/AboutNewTab.jsm"
);

// ASRouterTargeting.findMatchingMessage
add_task(async function find_matching_message() {
  const messages = [
    { id: "foo", targeting: "FOO" },
    { id: "bar", targeting: "!FOO" },
  ];
  const context = { FOO: true };

  const match = await ASRouterTargeting.findMatchingMessage({
    messages,
    context,
  });

  is(match, messages[0], "should match and return the correct message");
});

add_task(async function return_nothing_for_no_matching_message() {
  const messages = [{ id: "bar", targeting: "!FOO" }];
  const context = { FOO: true };

  const match = await ASRouterTargeting.findMatchingMessage({
    messages,
    context,
  });

  ok(!match, "should return nothing since no matching message exists");
});

add_task(async function check_other_error_handling() {
  let called = false;
  function onError(...args) {
    called = true;
  }

  const messages = [{ id: "foo", targeting: "foo" }];
  const context = {
    get foo() {
      throw new Error("test error");
    },
  };
  const match = await ASRouterTargeting.findMatchingMessage({
    messages,
    context,
    onError,
  });

  ok(!match, "should return nothing since no valid matching message exists");

  Assert.ok(called, "Attribute error caught");
});

// ASRouterTargeting.Environment
add_task(async function check_locale() {
  ok(
    Services.locale.appLocaleAsBCP47,
    "Services.locale.appLocaleAsBCP47 exists"
  );
  const message = {
    id: "foo",
    targeting: `locale == "${Services.locale.appLocaleAsBCP47}"`,
  };
  is(
    await ASRouterTargeting.findMatchingMessage({ messages: [message] }),
    message,
    "should select correct item when filtering by locale"
  );
});
add_task(async function check_localeLanguageCode() {
  const currentLanguageCode = Services.locale.appLocaleAsBCP47.substr(0, 2);
  is(
    Services.locale.negotiateLanguages(
      [currentLanguageCode],
      [Services.locale.appLocaleAsBCP47]
    )[0],
    Services.locale.appLocaleAsBCP47,
    "currentLanguageCode should resolve to the current locale (e.g en => en-US)"
  );
  const message = {
    id: "foo",
    targeting: `localeLanguageCode == "${currentLanguageCode}"`,
  };
  is(
    await ASRouterTargeting.findMatchingMessage({ messages: [message] }),
    message,
    "should select correct item when filtering by localeLanguageCode"
  );
});

add_task(async function checkProfileAgeCreated() {
  let profileAccessor = await ProfileAge();
  is(
    await ASRouterTargeting.Environment.profileAgeCreated,
    await profileAccessor.created,
    "should return correct profile age creation date"
  );

  const message = {
    id: "foo",
    targeting: `profileAgeCreated > ${(await profileAccessor.created) - 100}`,
  };
  is(
    await ASRouterTargeting.findMatchingMessage({ messages: [message] }),
    message,
    "should select correct item by profile age created"
  );
});

add_task(async function checkProfileAgeReset() {
  let profileAccessor = await ProfileAge();
  is(
    await ASRouterTargeting.Environment.profileAgeReset,
    await profileAccessor.reset,
    "should return correct profile age reset"
  );

  const message = {
    id: "foo",
    targeting: `profileAgeReset == ${await profileAccessor.reset}`,
  };
  is(
    await ASRouterTargeting.findMatchingMessage({ messages: [message] }),
    message,
    "should select correct item by profile age reset"
  );
});

add_task(async function checkCurrentDate() {
  let message = {
    id: "foo",
    targeting: `currentDate < '${new Date(Date.now() + 5000)}'|date`,
  };
  is(
    await ASRouterTargeting.findMatchingMessage({ messages: [message] }),
    message,
    "should select message based on currentDate < timestamp"
  );

  message = {
    id: "foo",
    targeting: `currentDate > '${new Date(Date.now() - 5000)}'|date`,
  };
  is(
    await ASRouterTargeting.findMatchingMessage({ messages: [message] }),
    message,
    "should select message based on currentDate > timestamp"
  );
});

add_task(async function check_usesFirefoxSync() {
  await pushPrefs(["services.sync.username", "someone@foo.com"]);
  is(
    await ASRouterTargeting.Environment.usesFirefoxSync,
    true,
    "should return true if a fx account is set"
  );

  const message = { id: "foo", targeting: "usesFirefoxSync" };
  is(
    await ASRouterTargeting.findMatchingMessage({ messages: [message] }),
    message,
    "should select correct item by usesFirefoxSync"
  );
});

add_task(async function check_isFxAEnabled() {
  await pushPrefs(["identity.fxaccounts.enabled", false]);
  is(
    await ASRouterTargeting.Environment.isFxAEnabled,
    false,
    "should return false if fxa is disabled"
  );

  const message = { id: "foo", targeting: "isFxAEnabled" };
  ok(
    !(await ASRouterTargeting.findMatchingMessage({ messages: [message] })),
    "should not select a message if fxa is disabled"
  );
});

add_task(async function check_isFxAEnabled() {
  await pushPrefs(["identity.fxaccounts.enabled", true]);
  is(
    await ASRouterTargeting.Environment.isFxAEnabled,
    true,
    "should return true if fxa is enabled"
  );

  const message = { id: "foo", targeting: "isFxAEnabled" };
  is(
    await ASRouterTargeting.findMatchingMessage({ messages: [message] }),
    message,
    "should select the correct message"
  );
});

add_task(async function check_totalBookmarksCount() {
  // Make sure we remove default bookmarks so they don't interfere
  await clearHistoryAndBookmarks();
  const message = { id: "foo", targeting: "totalBookmarksCount > 0" };

  const results = await ASRouterTargeting.findMatchingMessage({
    messages: [message],
  });
  ok(
    !(results ? JSON.stringify(results) : results),
    "Should not select any message because bookmarks count is not 0"
  );

  const bookmark = await PlacesUtils.bookmarks.insert({
    parentGuid: PlacesUtils.bookmarks.unfiledGuid,
    title: "foo",
    url: "https://mozilla1.com/nowNew",
  });

  QueryCache.queries.TotalBookmarksCount.expire();

  is(
    await ASRouterTargeting.findMatchingMessage({ messages: [message] }),
    message,
    "Should select correct item after bookmarks are added."
  );

  // Cleanup
  await PlacesUtils.bookmarks.remove(bookmark.guid);
});

add_task(async function check_needsUpdate() {
  QueryCache.queries.CheckBrowserNeedsUpdate.setUp(true);

  const message = { id: "foo", targeting: "needsUpdate" };

  is(
    await ASRouterTargeting.findMatchingMessage({ messages: [message] }),
    message,
    "Should select message because update count > 0"
  );

  QueryCache.queries.CheckBrowserNeedsUpdate.setUp(false);

  is(
    await ASRouterTargeting.findMatchingMessage({ messages: [message] }),
    null,
    "Should not select message because update count == 0"
  );
});

add_task(async function checksearchEngines() {
  const result = await ASRouterTargeting.Environment.searchEngines;
  const expectedInstalled = (await Services.search.getAppProvidedEngines())
    .map(engine => engine.identifier)
    .sort()
    .join(",");
  ok(
    result.installed.length,
    "searchEngines.installed should be a non-empty array"
  );
  is(
    result.installed.sort().join(","),
    expectedInstalled,
    "searchEngines.installed should be an array of visible search engines"
  );
  ok(
    result.current && typeof result.current === "string",
    "searchEngines.current should be a truthy string"
  );
  is(
    result.current,
    (await Services.search.getDefault()).identifier,
    "searchEngines.current should be the current engine name"
  );

  const message = {
    id: "foo",
    targeting: `searchEngines[.current == ${
      (await Services.search.getDefault()).identifier
    }]`,
  };
  is(
    await ASRouterTargeting.findMatchingMessage({ messages: [message] }),
    message,
    "should select correct item by searchEngines.current"
  );

  const message2 = {
    id: "foo",
    targeting: `searchEngines[${
      (await Services.search.getAppProvidedEngines())[0].identifier
    } in .installed]`,
  };
  is(
    await ASRouterTargeting.findMatchingMessage({ messages: [message2] }),
    message2,
    "should select correct item by searchEngines.installed"
  );
});

add_task(async function checkisDefaultBrowser() {
  const expected = ShellService.isDefaultBrowser();
  const result = ASRouterTargeting.Environment.isDefaultBrowser;
  is(typeof result, "boolean", "isDefaultBrowser should be a boolean value");
  is(
    result,
    expected,
    "isDefaultBrowser should be equal to ShellService.isDefaultBrowser()"
  );
  const message = {
    id: "foo",
    targeting: `isDefaultBrowser == ${expected.toString()}`,
  };
  is(
    await ASRouterTargeting.findMatchingMessage({ messages: [message] }),
    message,
    "should select correct item by isDefaultBrowser"
  );
});

add_task(async function checkdevToolsOpenedCount() {
  await pushPrefs(["devtools.selfxss.count", 5]);
  is(
    ASRouterTargeting.Environment.devToolsOpenedCount,
    5,
    "devToolsOpenedCount should be equal to devtools.selfxss.count pref value"
  );
  const message = { id: "foo", targeting: "devToolsOpenedCount >= 5" };
  is(
    await ASRouterTargeting.findMatchingMessage({ messages: [message] }),
    message,
    "should select correct item by devToolsOpenedCount"
  );
});

add_task(async function check_platformName() {
  const message = {
    id: "foo",
    targeting: `platformName == "${AppConstants.platform}"`,
  };
  is(
    await ASRouterTargeting.findMatchingMessage({ messages: [message] }),
    message,
    "should select correct item by platformName"
  );
});

AddonTestUtils.initMochitest(this);

add_task(async function checkAddonsInfo() {
  const FAKE_ID = "testaddon@tests.mozilla.org";
  const FAKE_NAME = "Test Addon";
  const FAKE_VERSION = "0.5.7";

  const xpi = AddonTestUtils.createTempWebExtensionFile({
    manifest: {
      applications: { gecko: { id: FAKE_ID } },
      name: FAKE_NAME,
      version: FAKE_VERSION,
    },
  });

  await Promise.all([
    AddonTestUtils.promiseWebExtensionStartup(FAKE_ID),
    AddonManager.installTemporaryAddon(xpi),
  ]);

  const { addons } = await AddonManager.getActiveAddons([
    "extension",
    "service",
  ]);

  const { addons: asRouterAddons, isFullData } = await ASRouterTargeting
    .Environment.addonsInfo;

  ok(
    addons.every(({ id }) => asRouterAddons[id]),
    "should contain every addon"
  );

  ok(
    Object.getOwnPropertyNames(asRouterAddons).every(id =>
      addons.some(addon => addon.id === id)
    ),
    "should contain no incorrect addons"
  );

  const testAddon = asRouterAddons[FAKE_ID];

  ok(
    Object.prototype.hasOwnProperty.call(testAddon, "version") &&
      testAddon.version === FAKE_VERSION,
    "should correctly provide `version` property"
  );

  ok(
    Object.prototype.hasOwnProperty.call(testAddon, "type") &&
      testAddon.type === "extension",
    "should correctly provide `type` property"
  );

  ok(
    Object.prototype.hasOwnProperty.call(testAddon, "isSystem") &&
      testAddon.isSystem === false,
    "should correctly provide `isSystem` property"
  );

  ok(
    Object.prototype.hasOwnProperty.call(testAddon, "isWebExtension") &&
      testAddon.isWebExtension === true,
    "should correctly provide `isWebExtension` property"
  );

  // As we installed our test addon the addons database must be initialised, so
  // (in this test environment) we expect to receive "full" data

  ok(isFullData, "should receive full data");

  ok(
    Object.prototype.hasOwnProperty.call(testAddon, "name") &&
      testAddon.name === FAKE_NAME,
    "should correctly provide `name` property from full data"
  );

  ok(
    Object.prototype.hasOwnProperty.call(testAddon, "userDisabled") &&
      testAddon.userDisabled === false,
    "should correctly provide `userDisabled` property from full data"
  );

  ok(
    Object.prototype.hasOwnProperty.call(testAddon, "installDate") &&
      Math.abs(Date.now() - new Date(testAddon.installDate)) < 60 * 1000,
    "should correctly provide `installDate` property from full data"
  );
});

add_task(async function checkFrecentSites() {
  const now = Date.now();
  const timeDaysAgo = numDays => now - numDays * 24 * 60 * 60 * 1000;

  const visits = [];
  for (const [uri, count, visitDate] of [
    ["https://mozilla1.com/", 10, timeDaysAgo(0)], // frecency 1000
    ["https://mozilla2.com/", 5, timeDaysAgo(1)], // frecency 500
    ["https://mozilla3.com/", 1, timeDaysAgo(2)], // frecency 100
  ]) {
    [...Array(count).keys()].forEach(() =>
      visits.push({
        uri,
        visitDate: visitDate * 1000, // Places expects microseconds
      })
    );
  }

  await PlacesTestUtils.addVisits(visits);

  let message = {
    id: "foo",
    targeting: "'mozilla3.com' in topFrecentSites|mapToProperty('host')",
  };
  is(
    await ASRouterTargeting.findMatchingMessage({ messages: [message] }),
    message,
    "should select correct item by host in topFrecentSites"
  );

  message = {
    id: "foo",
    targeting: "'non-existent.com' in topFrecentSites|mapToProperty('host')",
  };
  ok(
    !(await ASRouterTargeting.findMatchingMessage({ messages: [message] })),
    "should not select incorrect item by host in topFrecentSites"
  );

  message = {
    id: "foo",
    targeting:
      "'mozilla2.com' in topFrecentSites[.frecency >= 400]|mapToProperty('host')",
  };
  is(
    await ASRouterTargeting.findMatchingMessage({ messages: [message] }),
    message,
    "should select correct item when filtering by frecency"
  );

  message = {
    id: "foo",
    targeting:
      "'mozilla2.com' in topFrecentSites[.frecency >= 600]|mapToProperty('host')",
  };
  ok(
    !(await ASRouterTargeting.findMatchingMessage({ messages: [message] })),
    "should not select incorrect item when filtering by frecency"
  );

  message = {
    id: "foo",
    targeting: `'mozilla2.com' in topFrecentSites[.lastVisitDate >= ${timeDaysAgo(
      1
    ) - 1}]|mapToProperty('host')`,
  };
  is(
    await ASRouterTargeting.findMatchingMessage({ messages: [message] }),
    message,
    "should select correct item when filtering by lastVisitDate"
  );

  message = {
    id: "foo",
    targeting: `'mozilla2.com' in topFrecentSites[.lastVisitDate >= ${timeDaysAgo(
      0
    ) - 1}]|mapToProperty('host')`,
  };
  ok(
    !(await ASRouterTargeting.findMatchingMessage({ messages: [message] })),
    "should not select incorrect item when filtering by lastVisitDate"
  );

  message = {
    id: "foo",
    targeting: `(topFrecentSites[.frecency >= 900 && .lastVisitDate >= ${timeDaysAgo(
      1
    ) -
      1}]|mapToProperty('host') intersect ['mozilla3.com', 'mozilla2.com', 'mozilla1.com'])|length > 0`,
  };
  is(
    await ASRouterTargeting.findMatchingMessage({ messages: [message] }),
    message,
    "should select correct item when filtering by frecency and lastVisitDate with multiple candidate domains"
  );

  // Cleanup
  await clearHistoryAndBookmarks();
});

add_task(async function check_pinned_sites() {
  // Fresh profiles come with an empty set of pinned websites (pref doesn't
  // exist). Search shortcut topsites make this test more complicated because
  // the feature pins a new website on startup. Behaviour can vary when running
  // with --verify so it's more predictable to clear pins entirely.
  Services.prefs.clearUserPref("browser.newtabpage.pinned");
  NewTabUtils.pinnedLinks.resetCache();
  const originalPin = JSON.stringify(NewTabUtils.pinnedLinks.links);
  const sitesToPin = [
    { url: "https://foo.com" },
    { url: "https://bloo.com" },
    { url: "https://floogle.com", searchTopSite: true },
  ];
  sitesToPin.forEach(site =>
    NewTabUtils.pinnedLinks.pin(site, NewTabUtils.pinnedLinks.links.length)
  );

  // Unpinning adds null to the list of pinned sites, which we should test that we handle gracefully for our targeting
  NewTabUtils.pinnedLinks.unpin(sitesToPin[1]);
  ok(
    NewTabUtils.pinnedLinks.links.includes(null),
    "should have set an item in pinned links to null via unpinning for testing"
  );

  let message;

  message = {
    id: "foo",
    targeting: "'https://foo.com' in pinnedSites|mapToProperty('url')",
  };
  is(
    await ASRouterTargeting.findMatchingMessage({ messages: [message] }),
    message,
    "should select correct item by url in pinnedSites"
  );

  message = {
    id: "foo",
    targeting: "'foo.com' in pinnedSites|mapToProperty('host')",
  };
  is(
    await ASRouterTargeting.findMatchingMessage({ messages: [message] }),
    message,
    "should select correct item by host in pinnedSites"
  );

  message = {
    id: "foo",
    targeting:
      "'floogle.com' in pinnedSites[.searchTopSite == true]|mapToProperty('host')",
  };
  is(
    await ASRouterTargeting.findMatchingMessage({ messages: [message] }),
    message,
    "should select correct item by host and searchTopSite in pinnedSites"
  );

  // Cleanup
  sitesToPin.forEach(site => NewTabUtils.pinnedLinks.unpin(site));

  await clearHistoryAndBookmarks();
  Services.prefs.clearUserPref("browser.newtabpage.pinned");
  NewTabUtils.pinnedLinks.resetCache();
  is(
    JSON.stringify(NewTabUtils.pinnedLinks.links),
    originalPin,
    "should restore pinned sites to its original state"
  );
});

add_task(async function check_firefox_version() {
  const message = { id: "foo", targeting: "firefoxVersion > 0" };
  is(
    await ASRouterTargeting.findMatchingMessage({ messages: [message] }),
    message,
    "should select correct item when filtering by firefox version"
  );
});

add_task(async function check_region() {
  Region._setHomeRegion("DE", false);
  const message = { id: "foo", targeting: "region in ['DE']" };
  is(
    await ASRouterTargeting.findMatchingMessage({ messages: [message] }),
    message,
    "should select correct item when filtering by firefox geo"
  );
});

add_task(async function check_browserSettings() {
  is(
    await JSON.stringify(ASRouterTargeting.Environment.browserSettings.update),
    JSON.stringify(TelemetryEnvironment.currentEnvironment.settings.update),
    "should return correct update info"
  );
});

add_task(async function check_sync() {
  is(
    await ASRouterTargeting.Environment.sync.desktopDevices,
    Services.prefs.getIntPref("services.sync.clients.devices.desktop", 0),
    "should return correct desktopDevices info"
  );
  is(
    await ASRouterTargeting.Environment.sync.mobileDevices,
    Services.prefs.getIntPref("services.sync.clients.devices.mobile", 0),
    "should return correct mobileDevices info"
  );
  is(
    await ASRouterTargeting.Environment.sync.totalDevices,
    Services.prefs.getIntPref("services.sync.numClients", 0),
    "should return correct mobileDevices info"
  );
});

add_task(async function check_provider_cohorts() {
  await pushPrefs([
    "browser.newtabpage.activity-stream.asrouter.providers.onboarding",
    JSON.stringify({
      id: "onboarding",
      messages: [],
      enabled: true,
      cohort: "foo",
    }),
  ]);
  await pushPrefs([
    "browser.newtabpage.activity-stream.asrouter.providers.cfr",
    JSON.stringify({ id: "cfr", enabled: true, cohort: "bar" }),
  ]);
  is(
    await ASRouterTargeting.Environment.providerCohorts.onboarding,
    "foo",
    "should have cohort foo for onboarding"
  );
  is(
    await ASRouterTargeting.Environment.providerCohorts.cfr,
    "bar",
    "should have cohort bar for cfr"
  );
});

add_task(async function check_xpinstall_enabled() {
  // should default to true if pref doesn't exist
  is(await ASRouterTargeting.Environment.xpinstallEnabled, true);
  // flip to false, check targeting reflects that
  await pushPrefs(["xpinstall.enabled", false]);
  is(await ASRouterTargeting.Environment.xpinstallEnabled, false);
  // flip to true, check targeting reflects that
  await pushPrefs(["xpinstall.enabled", true]);
  is(await ASRouterTargeting.Environment.xpinstallEnabled, true);
});

add_task(async function check_pinned_tabs() {
  await BrowserTestUtils.withNewTab(
    { gBrowser, url: "about:blank" },
    async browser => {
      is(
        await ASRouterTargeting.Environment.hasPinnedTabs,
        false,
        "No pin tabs yet"
      );

      let tab = gBrowser.getTabForBrowser(browser);
      gBrowser.pinTab(tab);

      is(
        await ASRouterTargeting.Environment.hasPinnedTabs,
        true,
        "Should detect pinned tab"
      );

      gBrowser.unpinTab(tab);
    }
  );
});

add_task(async function check_hasAccessedFxAPanel() {
  is(
    await ASRouterTargeting.Environment.hasAccessedFxAPanel,
    false,
    "Not accessed yet"
  );

  await pushPrefs(["identity.fxaccounts.toolbar.accessed", true]);

  is(
    await ASRouterTargeting.Environment.hasAccessedFxAPanel,
    true,
    "Should detect panel access"
  );
});

add_task(async function checkCFRFeaturesUserPref() {
  await pushPrefs([
    "browser.newtabpage.activity-stream.asrouter.userprefs.cfr.features",
    false,
  ]);
  is(
    ASRouterTargeting.Environment.userPrefs.cfrFeatures,
    false,
    "cfrFeature should be false according to pref"
  );
  const message = { id: "foo", targeting: "userPrefs.cfrFeatures == false" };
  is(
    await ASRouterTargeting.findMatchingMessage({ messages: [message] }),
    message,
    "should select correct item by cfrFeature"
  );
});

add_task(async function checkCFRAddonsUserPref() {
  await pushPrefs([
    "browser.newtabpage.activity-stream.asrouter.userprefs.cfr.addons",
    false,
  ]);
  is(
    ASRouterTargeting.Environment.userPrefs.cfrAddons,
    false,
    "cfrFeature should be false according to pref"
  );
  const message = { id: "foo", targeting: "userPrefs.cfrAddons == false" };
  is(
    await ASRouterTargeting.findMatchingMessage({ messages: [message] }),
    message,
    "should select correct item by cfrAddons"
  );
});

add_task(async function check_blockedCountByType() {
  const message = {
    id: "foo",
    targeting:
      "blockedCountByType.cryptominerCount == 0 && blockedCountByType.socialCount == 0",
  };

  is(
    await ASRouterTargeting.findMatchingMessage({ messages: [message] }),
    message,
    "should select correct item"
  );
});

add_task(async function checkPatternMatches() {
  const now = Date.now();
  const timeMinutesAgo = numMinutes => now - numMinutes * 60 * 1000;
  const messages = [
    {
      id: "message_with_pattern",
      targeting: "true",
      trigger: { id: "frequentVisits", patterns: ["*://*.github.com/"] },
    },
  ];
  const trigger = {
    id: "frequentVisits",
    context: {
      recentVisits: [
        { timestamp: timeMinutesAgo(33) },
        { timestamp: timeMinutesAgo(17) },
        { timestamp: timeMinutesAgo(1) },
      ],
    },
    param: { host: "github.com", url: "https://gist.github.com" },
  };

  is(
    (await ASRouterTargeting.findMatchingMessage({ messages, trigger })).id,
    "message_with_pattern",
    "should select PIN_TAB mesage"
  );
});

add_task(async function checkPatternsValid() {
  const messages = (await CFRMessageProvider.getMessages()).filter(
    m => m.trigger?.patterns
  );

  for (const message of messages) {
    Assert.ok(new MatchPatternSet(message.trigger.patterns));
  }
});

add_task(async function check_isChinaRepack() {
  const prefDefaultBranch = Services.prefs.getDefaultBranch("distribution.");
  const messages = [
    { id: "msg_for_china_repack", targeting: "isChinaRepack == true" },
    { id: "msg_for_everyone_else", targeting: "isChinaRepack == false" },
  ];

  is(
    await ASRouterTargeting.Environment.isChinaRepack,
    false,
    "Fx w/o partner repack info set is not China repack"
  );
  is(
    (await ASRouterTargeting.findMatchingMessage({ messages })).id,
    "msg_for_everyone_else",
    "should select the message for non China repack users"
  );

  prefDefaultBranch.setCharPref("id", "MozillaOnline");

  is(
    await ASRouterTargeting.Environment.isChinaRepack,
    true,
    "Fx with `distribution.id` set to `MozillaOnline` is China repack"
  );
  is(
    (await ASRouterTargeting.findMatchingMessage({ messages })).id,
    "msg_for_china_repack",
    "should select the message for China repack users"
  );

  prefDefaultBranch.setCharPref("id", "Example");

  is(
    await ASRouterTargeting.Environment.isChinaRepack,
    false,
    "Fx with `distribution.id` set to other string is not China repack"
  );
  is(
    (await ASRouterTargeting.findMatchingMessage({ messages })).id,
    "msg_for_everyone_else",
    "should select the message for non China repack users"
  );

  prefDefaultBranch.deleteBranch("");
});

add_task(async function check_userId() {
  await SpecialPowers.pushPrefEnv({
    set: [["app.normandy.user_id", "foo123"]],
  });
  is(
    await ASRouterTargeting.Environment.userId,
    "foo123",
    "should read userID from normandy user id pref"
  );
});

add_task(async function check_profileRestartCount() {
  ok(
    !isNaN(ASRouterTargeting.Environment.profileRestartCount),
    "it should return a number"
  );
});

add_task(async function check_homePageSettings_default() {
  let settings = ASRouterTargeting.Environment.homePageSettings;

  ok(settings.isDefault, "should set as default");
  ok(!settings.isLocked, "should not set as locked");
  ok(!settings.isWebExt, "should not be web extension");
  ok(!settings.isCustomUrl, "should not be custom URL");
  is(settings.urls.length, 1, "should be an 1-entry array");
  is(settings.urls[0].url, "about:home", "should be about:home");
  is(settings.urls[0].host, "", "should be an empty string");
});

add_task(async function check_homePageSettings_locked() {
  const PREF = "browser.startup.homepage";
  Services.prefs.lockPref(PREF);
  let settings = ASRouterTargeting.Environment.homePageSettings;

  ok(settings.isDefault, "should set as default");
  ok(settings.isLocked, "should set as locked");
  ok(!settings.isWebExt, "should not be web extension");
  ok(!settings.isCustomUrl, "should not be custom URL");
  is(settings.urls.length, 1, "should be an 1-entry array");
  is(settings.urls[0].url, "about:home", "should be about:home");
  is(settings.urls[0].host, "", "should be an empty string");
  Services.prefs.unlockPref(PREF);
});

add_task(async function check_homePageSettings_customURL() {
  await HomePage.set("https://www.google.com");
  let settings = ASRouterTargeting.Environment.homePageSettings;

  ok(!settings.isDefault, "should not be the default");
  ok(!settings.isLocked, "should set as locked");
  ok(!settings.isWebExt, "should not be web extension");
  ok(settings.isCustomUrl, "should be custom URL");
  is(settings.urls.length, 1, "should be an 1-entry array");
  is(settings.urls[0].url, "https://www.google.com", "should be a custom URL");
  is(
    settings.urls[0].host,
    "google.com",
    "should be the host name without 'www.'"
  );

  HomePage.reset();
});

add_task(async function check_homePageSettings_customURL_multiple() {
  await HomePage.set("https://www.google.com|https://www.youtube.com");
  let settings = ASRouterTargeting.Environment.homePageSettings;

  ok(!settings.isDefault, "should not be the default");
  ok(!settings.isLocked, "should not set as locked");
  ok(!settings.isWebExt, "should not be web extension");
  ok(settings.isCustomUrl, "should be custom URL");
  is(settings.urls.length, 2, "should be a 2-entry array");
  is(settings.urls[0].url, "https://www.google.com", "should be a custom URL");
  is(
    settings.urls[0].host,
    "google.com",
    "should be the host name without 'www.'"
  );
  is(settings.urls[1].url, "https://www.youtube.com", "should be a custom URL");
  is(
    settings.urls[1].host,
    "youtube.com",
    "should be the host name without 'www.'"
  );

  HomePage.reset();
});

add_task(async function check_homePageSettings_webExtension() {
  const extURI =
    "moz-extension://0d735548-ba3c-aa43-a0e4-7089584fbb53/homepage.html";
  await HomePage.set(extURI);
  let settings = ASRouterTargeting.Environment.homePageSettings;

  ok(!settings.isDefault, "should not be the default");
  ok(!settings.isLocked, "should not set as locked");
  ok(settings.isWebExt, "should be a web extension");
  ok(!settings.isCustomUrl, "should be custom URL");
  is(settings.urls.length, 1, "should be an 1-entry array");
  is(settings.urls[0].url, extURI, "should be a webExtension URI");
  is(settings.urls[0].host, "", "should be an empty string");

  HomePage.reset();
});

add_task(async function check_newtabSettings_default() {
  let settings = ASRouterTargeting.Environment.newtabSettings;

  ok(settings.isDefault, "should set as default");
  ok(!settings.isWebExt, "should not be web extension");
  ok(!settings.isCustomUrl, "should not be custom URL");
  is(settings.url, "about:newtab", "should be about:home");
  is(settings.host, "", "should be an empty string");
});

add_task(async function check_newTabSettings_customURL() {
  AboutNewTab.newTabURL = "https://www.google.com";
  let settings = ASRouterTargeting.Environment.newtabSettings;

  ok(!settings.isDefault, "should not be the default");
  ok(!settings.isWebExt, "should not be web extension");
  ok(settings.isCustomUrl, "should be custom URL");
  is(settings.url, "https://www.google.com", "should be a custom URL");
  is(settings.host, "google.com", "should be the host name without 'www.'");

  AboutNewTab.resetNewTabURL();
});

add_task(async function check_newTabSettings_webExtension() {
  const extURI =
    "moz-extension://0d735548-ba3c-aa43-a0e4-7089584fbb53/homepage.html";
  AboutNewTab.newTabURL = extURI;
  let settings = ASRouterTargeting.Environment.newtabSettings;

  ok(!settings.isDefault, "should not be the default");
  ok(settings.isWebExt, "should not be web extension");
  ok(!settings.isCustomUrl, "should be custom URL");
  is(settings.url, extURI, "should be the web extension URI");
  is(settings.host, "", "should be an empty string");

  AboutNewTab.resetNewTabURL();
});

add_task(async function check_openUrlTrigger_context() {
  const message = {
    ...(await CFRMessageProvider.getMessages()).find(
      m => m.id === "YOUTUBE_ENHANCE_3"
    ),
    targeting: "visitsCount == 3",
  };
  const trigger = {
    id: "openURL",
    context: { visitsCount: 3 },
    param: { host: "youtube.com", url: "https://www.youtube.com" },
  };

  is(
    (
      await ASRouterTargeting.findMatchingMessage({
        messages: [message],
        trigger,
      })
    ).id,
    message.id,
    `should select ${message.id} mesage`
  );
});

add_task(async function check_is_major_upgrade() {
  let message = {
    id: "check_is_major_upgrade",
    targeting: `isMajorUpgrade != undefined && isMajorUpgrade == ${
      Cc["@mozilla.org/browser/clh;1"].getService(Ci.nsIBrowserHandler)
        .majorUpgrade
    }`,
  };

  is(
    (await ASRouterTargeting.findMatchingMessage({ messages: [message] })).id,
    message.id,
    "Should select the message"
  );
});

add_task(async function check_userMonthlyActivity() {
  ok(
    Array.isArray(await ASRouterTargeting.Environment.userMonthlyActivity),
    "value is an array"
  );
});

add_task(async function check_doesAppNeedPing() {
  is(
    typeof (await ASRouterTargeting.Environment.doesAppNeedPin),
    "boolean",
    "Should return a boolean"
  );
});
