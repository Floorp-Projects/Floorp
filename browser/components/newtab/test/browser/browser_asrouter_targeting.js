const {ASRouterTargeting, QueryCache} =
  ChromeUtils.import("resource://activity-stream/lib/ASRouterTargeting.jsm");
const {AddonTestUtils} =
  ChromeUtils.import("resource://testing-common/AddonTestUtils.jsm");
const {CFRMessageProvider} = ChromeUtils.import("resource://activity-stream/lib/CFRMessageProvider.jsm");
ChromeUtils.defineModuleGetter(this, "ProfileAge",
  "resource://gre/modules/ProfileAge.jsm");
ChromeUtils.defineModuleGetter(this, "AddonManager",
  "resource://gre/modules/AddonManager.jsm");
ChromeUtils.defineModuleGetter(this, "ShellService",
  "resource:///modules/ShellService.jsm");
ChromeUtils.defineModuleGetter(this, "NewTabUtils",
  "resource://gre/modules/NewTabUtils.jsm");
ChromeUtils.defineModuleGetter(this, "PlacesTestUtils",
  "resource://testing-common/PlacesTestUtils.jsm");
ChromeUtils.defineModuleGetter(this, "TelemetryEnvironment",
  "resource://gre/modules/TelemetryEnvironment.jsm");

// ASRouterTargeting.isMatch
add_task(async function should_do_correct_targeting() {
  is(await ASRouterTargeting.isMatch("FOO", {FOO: true}), true, "should return true for a matching value");
  is(await ASRouterTargeting.isMatch("!FOO", {FOO: true}), false, "should return false for a non-matching value");
});

add_task(async function should_handle_async_getters() {
  const context = {get FOO() { return Promise.resolve(true); }};
  is(await ASRouterTargeting.isMatch("FOO", context), true, "should return true for a matching async value");
});

// ASRouterTargeting.findMatchingMessage
add_task(async function find_matching_message() {
  const messages = [
    {id: "foo", targeting: "FOO"},
    {id: "bar", targeting: "!FOO"},
  ];
  const context = {FOO: true};

  const match = await ASRouterTargeting.findMatchingMessage({messages, context});

  is(match, messages[0], "should match and return the correct message");
});

add_task(async function return_nothing_for_no_matching_message() {
  const messages = [{id: "bar", targeting: "!FOO"}];
  const context = {FOO: true};

  const match = await ASRouterTargeting.findMatchingMessage({messages, context});

  is(match, undefined, "should return nothing since no matching message exists");
});

add_task(async function check_syntax_error_handling() {
  let result;
  function onError(...args) {
    result = args;
  }

  const messages = [{id: "foo", targeting: "foo === 0"}];
  const match = await ASRouterTargeting.findMatchingMessage({messages, onError});

  is(match, undefined, "should return nothing since no valid matching message exists");
  // Note that in order for the following test to pass, we are expecting a particular filepath for mozjexl.
  // If the location of this file has changed, the MOZ_JEXL_FILEPATH constant should be updated om ASRouterTargeting.jsm
  is(result[0], ASRouterTargeting.ERROR_TYPES.MALFORMED_EXPRESSION,
    "should recognize the error as coming from mozjexl and call onError with the MALFORMED_EXPRESSION error type");
  ok(result[1].message,
    "should call onError with the error from mozjexl");
  is(result[2], messages[0],
    "should call onError with the invalid message");
});

add_task(async function check_other_error_handling() {
  let result;
  function onError(...args) {
    result = args;
  }

  const messages = [{id: "foo", targeting: "foo"}];
  const context = {get foo() { throw new Error("test error"); }};
  const match = await ASRouterTargeting.findMatchingMessage({messages, context, onError});

  is(match, undefined, "should return nothing since no valid matching message exists");
  // Note that in order for the following test to pass, we are expecting a particular filepath for mozjexl.
  // If the location of this file has changed, the MOZ_JEXL_FILEPATH constant should be updated om ASRouterTargeting.jsm
  is(result[0], ASRouterTargeting.ERROR_TYPES.OTHER_ERROR,
    "should not recognize the error as being an other error, not a mozjexl one");
  is(result[1].message, "test error",
    "should call onError with the error thrown in the context");
  is(result[2], messages[0],
    "should call onError with the invalid message");
});

// ASRouterTargeting.Environment
add_task(async function check_locale() {
  ok(Services.locale.appLocaleAsLangTag, "Services.locale.appLocaleAsLangTag exists");
  const message = {id: "foo", targeting: `locale == "${Services.locale.appLocaleAsLangTag}"`};
  is(await ASRouterTargeting.findMatchingMessage({messages: [message]}), message,
    "should select correct item when filtering by locale");
});
add_task(async function check_localeLanguageCode() {
  const currentLanguageCode = Services.locale.appLocaleAsLangTag.substr(0, 2);
  is(
    Services.locale.negotiateLanguages([currentLanguageCode], [Services.locale.appLocaleAsLangTag])[0],
    Services.locale.appLocaleAsLangTag,
    "currentLanguageCode should resolve to the current locale (e.g en => en-US)"
  );
  const message = {id: "foo", targeting: `localeLanguageCode == "${currentLanguageCode}"`};
  is(await ASRouterTargeting.findMatchingMessage({messages: [message]}), message,
    "should select correct item when filtering by localeLanguageCode");
});

add_task(async function checkProfileAgeCreated() {
  let profileAccessor = await ProfileAge();
  is(await ASRouterTargeting.Environment.profileAgeCreated, await profileAccessor.created,
    "should return correct profile age creation date");

  const message = {id: "foo", targeting: `profileAgeCreated > ${await profileAccessor.created - 100}`};
  is(await ASRouterTargeting.findMatchingMessage({messages: [message]}), message,
    "should select correct item by profile age created");
});

add_task(async function checkProfileAgeReset() {
  let profileAccessor = await ProfileAge();
  is(await ASRouterTargeting.Environment.profileAgeReset, await profileAccessor.reset,
    "should return correct profile age reset");

  const message = {id: "foo", targeting: `profileAgeReset == ${await profileAccessor.reset}`};
  is(await ASRouterTargeting.findMatchingMessage({messages: [message]}), message,
    "should select correct item by profile age reset");
});

add_task(async function checkCurrentDate() {
  let message = {id: "foo", targeting: `currentDate < '${new Date(Date.now() + 5000)}'|date`};
  is(await ASRouterTargeting.findMatchingMessage({messages: [message]}), message,
    "should select message based on currentDate < timestamp");

  message = {id: "foo", targeting: `currentDate > '${new Date(Date.now() - 5000)}'|date`};
  is(await ASRouterTargeting.findMatchingMessage({messages: [message]}), message,
    "should select message based on currentDate > timestamp");
});

add_task(async function check_usesFirefoxSync() {
  await pushPrefs(["services.sync.username", "someone@foo.com"]);
  is(await ASRouterTargeting.Environment.usesFirefoxSync, true,
    "should return true if a fx account is set");

  const message = {id: "foo", targeting: "usesFirefoxSync"};
  is(await ASRouterTargeting.findMatchingMessage({messages: [message]}), message,
    "should select correct item by usesFirefoxSync");
});

add_task(async function check_isFxAEnabled() {
  await pushPrefs(["identity.fxaccounts.enabled", false]);
  is(await ASRouterTargeting.Environment.isFxAEnabled, false,
    "should return false if fxa is disabled");

  const message = {id: "foo", targeting: "isFxAEnabled"};
  is(await ASRouterTargeting.findMatchingMessage({messages: [message]}), undefined,
    "should not select a message if fxa is disabled");
});

add_task(async function check_isFxAEnabled() {
  await pushPrefs(["identity.fxaccounts.enabled", true]);
  is(await ASRouterTargeting.Environment.isFxAEnabled, true,
    "should return true if fxa is enabled");

  const message = {id: "foo", targeting: "isFxAEnabled"};
  is(await ASRouterTargeting.findMatchingMessage({messages: [message]}), message,
    "should select the correct message");
});

add_task(async function check_totalBookmarksCount() {
  // Make sure we remove default bookmarks so they don't interfere
  await clearHistoryAndBookmarks();
  const message = {id: "foo", targeting: "totalBookmarksCount > 0"};

  const results = await ASRouterTargeting.findMatchingMessage({messages: [message]});
  is(results ? JSON.stringify(results) : results, undefined,
    "Should not select any message because bookmarks count is not 0");

  const bookmark = await PlacesUtils.bookmarks.insert({
    parentGuid: PlacesUtils.bookmarks.unfiledGuid,
    title: "foo",
    url: "https://mozilla1.com/nowNew",
  });

  QueryCache.queries.TotalBookmarksCount.expire();

  is(await ASRouterTargeting.findMatchingMessage({messages: [message]}), message,
    "Should select correct item after bookmarks are added.");

  // Cleanup
  await PlacesUtils.bookmarks.remove(bookmark.guid);
});

add_task(async function check_needsUpdate() {
  QueryCache.queries.CheckBrowserNeedsUpdate.setUp(true);

  const message = {id: "foo", targeting: "needsUpdate"};

  is(await ASRouterTargeting.findMatchingMessage({messages: [message]}), message,
    "Should select message because update count > 0");

  QueryCache.queries.CheckBrowserNeedsUpdate.setUp(false);

  is(await ASRouterTargeting.findMatchingMessage({messages: [message]}), null,
    "Should not select message because update count == 0");
});

add_task(async function checksearchEngines() {
  const result = await ASRouterTargeting.Environment.searchEngines;
  const expectedInstalled = (await Services.search.getVisibleEngines())
    .map(engine => engine.identifier)
    .sort()
    .join(",");
  ok(result.installed.length,
    "searchEngines.installed should be a non-empty array");
  is(result.installed.sort().join(","), expectedInstalled,
    "searchEngines.installed should be an array of visible search engines");
  ok(result.current && typeof result.current === "string",
    "searchEngines.current should be a truthy string");
  is(result.current, (await Services.search.getDefault()).identifier,
    "searchEngines.current should be the current engine name");

  const message = {id: "foo", targeting: `searchEngines[.current == ${(await Services.search.getDefault()).identifier}]`};
  is(await ASRouterTargeting.findMatchingMessage({messages: [message]}), message,
    "should select correct item by searchEngines.current");

  const message2 = {id: "foo", targeting: `searchEngines[${(await Services.search.getVisibleEngines())[0].identifier} in .installed]`};
  is(await ASRouterTargeting.findMatchingMessage({messages: [message2]}), message2,
    "should select correct item by searchEngines.installed");
});

add_task(async function checkisDefaultBrowser() {
  const expected = ShellService.isDefaultBrowser();
  const result = ASRouterTargeting.Environment.isDefaultBrowser;
  is(typeof result, "boolean",
    "isDefaultBrowser should be a boolean value");
  is(result, expected,
    "isDefaultBrowser should be equal to ShellService.isDefaultBrowser()");
  const message = {id: "foo", targeting: `isDefaultBrowser == ${expected.toString()}`};
  is(await ASRouterTargeting.findMatchingMessage({messages: [message]}), message,
    "should select correct item by isDefaultBrowser");
});

add_task(async function checkdevToolsOpenedCount() {
  await pushPrefs(["devtools.selfxss.count", 5]);
  is(ASRouterTargeting.Environment.devToolsOpenedCount, 5,
    "devToolsOpenedCount should be equal to devtools.selfxss.count pref value");
  const message = {id: "foo", targeting: "devToolsOpenedCount >= 5"};
  is(await ASRouterTargeting.findMatchingMessage({messages: [message]}), message,
    "should select correct item by devToolsOpenedCount");
});

AddonTestUtils.initMochitest(this);

add_task(async function checkAddonsInfo() {
  const FAKE_ID = "testaddon@tests.mozilla.org";
  const FAKE_NAME = "Test Addon";
  const FAKE_VERSION = "0.5.7";

  const xpi = AddonTestUtils.createTempWebExtensionFile({
    manifest: {
      applications: {gecko: {id: FAKE_ID}},
      name: FAKE_NAME,
      version: FAKE_VERSION,
    },
  });

  await Promise.all([
    AddonTestUtils.promiseWebExtensionStartup(FAKE_ID),
    AddonManager.installTemporaryAddon(xpi),
  ]);

  const {addons} = await AddonManager.getActiveAddons(["extension", "service"]);

  const {addons: asRouterAddons, isFullData} = await ASRouterTargeting.Environment.addonsInfo;

  ok(addons.every(({id}) => asRouterAddons[id]), "should contain every addon");

  ok(Object.getOwnPropertyNames(asRouterAddons).every(id => addons.some(addon => addon.id === id)),
    "should contain no incorrect addons");

  const testAddon = asRouterAddons[FAKE_ID];

  ok(Object.prototype.hasOwnProperty.call(testAddon, "version") && testAddon.version === FAKE_VERSION,
    "should correctly provide `version` property");

  ok(Object.prototype.hasOwnProperty.call(testAddon, "type") && testAddon.type === "extension",
    "should correctly provide `type` property");

  ok(Object.prototype.hasOwnProperty.call(testAddon, "isSystem") && testAddon.isSystem === false,
    "should correctly provide `isSystem` property");

  ok(Object.prototype.hasOwnProperty.call(testAddon, "isWebExtension") && testAddon.isWebExtension === true,
    "should correctly provide `isWebExtension` property");

  // As we installed our test addon the addons database must be initialised, so
  // (in this test environment) we expect to receive "full" data

  ok(isFullData, "should receive full data");

  ok(Object.prototype.hasOwnProperty.call(testAddon, "name") && testAddon.name === FAKE_NAME,
    "should correctly provide `name` property from full data");

  ok(Object.prototype.hasOwnProperty.call(testAddon, "userDisabled") && testAddon.userDisabled === false,
    "should correctly provide `userDisabled` property from full data");

  ok(Object.prototype.hasOwnProperty.call(testAddon, "installDate") &&
    (Math.abs(Date.now() - new Date(testAddon.installDate)) < 60 * 1000),
    "should correctly provide `installDate` property from full data");
});

add_task(async function checkFrecentSites() {
  const now = Date.now();
  const timeDaysAgo = numDays => now - numDays * 24 * 60 * 60 * 1000;

  const visits = [];
  for (const [uri, count, visitDate] of [
    ["https://mozilla1.com/", 10, timeDaysAgo(0)], // frecency 1000
    ["https://mozilla2.com/", 5, timeDaysAgo(1)],  // frecency 500
    ["https://mozilla3.com/", 1, timeDaysAgo(2)],   // frecency 100
  ]) {
    [...Array(count).keys()].forEach(() => visits.push({
      uri,
      visitDate: visitDate * 1000, // Places expects microseconds
    }));
  }

  await PlacesTestUtils.addVisits(visits);

  let message = {id: "foo", targeting: "'mozilla3.com' in topFrecentSites|mapToProperty('host')"};
  is(await ASRouterTargeting.findMatchingMessage({messages: [message]}), message,
    "should select correct item by host in topFrecentSites");

  message = {id: "foo", targeting: "'non-existent.com' in topFrecentSites|mapToProperty('host')"};
  is(await ASRouterTargeting.findMatchingMessage({messages: [message]}), undefined,
    "should not select incorrect item by host in topFrecentSites");

  message = {id: "foo", targeting: "'mozilla2.com' in topFrecentSites[.frecency >= 400]|mapToProperty('host')"};
  is(await ASRouterTargeting.findMatchingMessage({messages: [message]}), message,
    "should select correct item when filtering by frecency");

  message = {id: "foo", targeting: "'mozilla2.com' in topFrecentSites[.frecency >= 600]|mapToProperty('host')"};
  is(await ASRouterTargeting.findMatchingMessage({messages: [message]}), undefined,
    "should not select incorrect item when filtering by frecency");

  message = {id: "foo", targeting: `'mozilla2.com' in topFrecentSites[.lastVisitDate >= ${timeDaysAgo(1) - 1}]|mapToProperty('host')`};
  is(await ASRouterTargeting.findMatchingMessage({messages: [message]}), message,
    "should select correct item when filtering by lastVisitDate");

  message = {id: "foo", targeting: `'mozilla2.com' in topFrecentSites[.lastVisitDate >= ${timeDaysAgo(0) - 1}]|mapToProperty('host')`};
  is(await ASRouterTargeting.findMatchingMessage({messages: [message]}), undefined,
    "should not select incorrect item when filtering by lastVisitDate");

  message = {id: "foo", targeting: `(topFrecentSites[.frecency >= 900 && .lastVisitDate >= ${timeDaysAgo(1) - 1}]|mapToProperty('host') intersect ['mozilla3.com', 'mozilla2.com', 'mozilla1.com'])|length > 0`};
  is(await ASRouterTargeting.findMatchingMessage({messages: [message]}), message,
    "should select correct item when filtering by frecency and lastVisitDate with multiple candidate domains");

  // Cleanup
  await clearHistoryAndBookmarks();
});

add_task(async function check_pinned_sites() {
  const originalPin = JSON.stringify(NewTabUtils.pinnedLinks.links);
  const sitesToPin = [
    {url: "https://foo.com"},
    {url: "https://bloo.com"},
    {url: "https://floogle.com", searchTopSite: true},
  ];
  sitesToPin.forEach((site => NewTabUtils.pinnedLinks.pin(site, NewTabUtils.pinnedLinks.links.length)));

  // Unpinning adds null to the list of pinned sites, which we should test that we handle gracefully for our targeting
  NewTabUtils.pinnedLinks.unpin(sitesToPin[1]);
  ok(NewTabUtils.pinnedLinks.links.includes(null),
    "should have set an item in pinned links to null via unpinning for testing");

  let message;

  message = {id: "foo", targeting: "'https://foo.com' in pinnedSites|mapToProperty('url')"};
  is(await ASRouterTargeting.findMatchingMessage({messages: [message]}), message,
    "should select correct item by url in pinnedSites");

  message = {id: "foo", targeting: "'foo.com' in pinnedSites|mapToProperty('host')"};
  is(await ASRouterTargeting.findMatchingMessage({messages: [message]}), message,
    "should select correct item by host in pinnedSites");

  message = {id: "foo", targeting: "'floogle.com' in pinnedSites[.searchTopSite == true]|mapToProperty('host')"};
  is(await ASRouterTargeting.findMatchingMessage({messages: [message]}), message,
    "should select correct item by host and searchTopSite in pinnedSites");

  // Cleanup
  sitesToPin.forEach(site => NewTabUtils.pinnedLinks.unpin(site));

  await clearHistoryAndBookmarks();
  is(JSON.stringify(NewTabUtils.pinnedLinks.links), originalPin,
    "should restore pinned sites to its original state");
});

add_task(async function check_firefox_version() {
  const message = {id: "foo", targeting: "firefoxVersion > 0"};
  is(await ASRouterTargeting.findMatchingMessage({messages: [message]}), message,
    "should select correct item when filtering by firefox version");
});

add_task(async function check_region() {
  await SpecialPowers.pushPrefEnv({"set": [["browser.search.region", "DE"]]});

  const message = {id: "foo", targeting: "region in ['DE']"};
  is(await ASRouterTargeting.findMatchingMessage({messages: [message]}), message,
    "should select correct item when filtering by firefox geo");
});

add_task(async function check_browserSettings() {
  is(await JSON.stringify(ASRouterTargeting.Environment.browserSettings.update), JSON.stringify(TelemetryEnvironment.currentEnvironment.settings.update),
      "should return correct update info");
});

add_task(async function check_sync() {
  is(await ASRouterTargeting.Environment.sync.desktopDevices, Services.prefs.getIntPref("services.sync.clients.devices.desktop", 0),
    "should return correct desktopDevices info");
  is(await ASRouterTargeting.Environment.sync.mobileDevices, Services.prefs.getIntPref("services.sync.clients.devices.mobile", 0),
    "should return correct mobileDevices info");
  is(await ASRouterTargeting.Environment.sync.totalDevices, Services.prefs.getIntPref("services.sync.numClients", 0),
    "should return correct mobileDevices info");
});

add_task(async function check_provider_cohorts() {
  await pushPrefs(["browser.newtabpage.activity-stream.asrouter.providers.onboarding", JSON.stringify({id: "onboarding", messages: [], enabled: true, cohort: "foo"})]);
  await pushPrefs(["browser.newtabpage.activity-stream.asrouter.providers.cfr", JSON.stringify({id: "cfr", enabled: true, cohort: "bar"})]);
  is(await ASRouterTargeting.Environment.providerCohorts.onboarding, "foo",
    "should have cohort foo for onboarding");
  is(await ASRouterTargeting.Environment.providerCohorts.cfr, "bar",
    "should have cohort bar for cfr");
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
  await BrowserTestUtils.withNewTab({gBrowser, url: "about:blank"}, async browser => {
    is(await ASRouterTargeting.Environment.hasPinnedTabs, false, "No pin tabs yet");

    let tab = gBrowser.getTabForBrowser(browser);
    gBrowser.pinTab(tab);

    is(await ASRouterTargeting.Environment.hasPinnedTabs, true, "Should detect pinned tab");

    gBrowser.unpinTab(tab);
  });
});

add_task(async function checkCFRPinnedTabsTargetting() {
  const now = Date.now();
  const timeMinutesAgo = numMinutes => now - numMinutes * 60 * 1000;
  const messages = CFRMessageProvider.getMessages();
  const trigger = {
    id: "frequentVisits",
    context: {
      recentVisits: [
        {timestamp: timeMinutesAgo(61)},
        {timestamp: timeMinutesAgo(30)},
        {timestamp: timeMinutesAgo(1)},
      ],
    },
    param: {host: "github.com", url: "https://google.com"},
  };

  is(await ASRouterTargeting.findMatchingMessage({messages, trigger}), undefined,
    "should not select PIN_TAB mesage with only 2 visits in past hour");

  trigger.context.recentVisits.push({timestamp: timeMinutesAgo(59)});
  is((await ASRouterTargeting.findMatchingMessage({messages, trigger})).id, "PIN_TAB",
    "should select PIN_TAB mesage");

  await BrowserTestUtils.withNewTab({gBrowser, url: "about:blank"}, async browser => {
    let tab = gBrowser.getTabForBrowser(browser);
    gBrowser.pinTab(tab);
    is(await ASRouterTargeting.findMatchingMessage({messages, trigger}), undefined,
      "should not select PIN_TAB mesage if there is a pinned tab already");
    gBrowser.unpinTab(tab);
  });

  trigger.param = {host: "foo.bar", url: "https://foo.bar"};
  is(await ASRouterTargeting.findMatchingMessage({messages, trigger}), undefined,
    "should not select PIN_TAB mesage with a trigger param/host not in our hostlist");
});

add_task(async function checkPatternMatches() {
  const now = Date.now();
  const timeMinutesAgo = numMinutes => now - numMinutes * 60 * 1000;
  const messages = [{id: "message_with_pattern", targeting: "true", trigger: {id: "frequentVisits", patterns: ["*://*.github.com/"]}}];
  const trigger = {
    id: "frequentVisits",
    context: {
      recentVisits: [
        {timestamp: timeMinutesAgo(33)},
        {timestamp: timeMinutesAgo(17)},
        {timestamp: timeMinutesAgo(1)},
      ],
    },
    param: {host: "github.com", url: "https://gist.github.com"},
  };

  is((await ASRouterTargeting.findMatchingMessage({messages, trigger})).id, "message_with_pattern", "should select PIN_TAB mesage");
});

add_task(async function checkPatternsValid() {
  const messages = CFRMessageProvider.getMessages().filter(m => m.trigger.patterns);

  for (const message of messages) {
    Assert.ok(new MatchPatternSet(message.trigger.patterns));
  }
});
