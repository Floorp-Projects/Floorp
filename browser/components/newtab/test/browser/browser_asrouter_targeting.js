ChromeUtils.defineModuleGetter(this, "ASRouterTargeting",
"resource://activity-stream/lib/ASRouterTargeting.jsm");
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

const {AddonTestUtils} = ChromeUtils.import("resource://testing-common/AddonTestUtils.jsm", {});

// ASRouterTargeting.isMatch
add_task(async function should_do_correct_targeting() {
  is(await ASRouterTargeting.isMatch("FOO", {}, {FOO: true}), true, "should return true for a matching value");
  is(await ASRouterTargeting.isMatch("!FOO", {}, {FOO: true}), false, "should return false for a non-matching value");
});

add_task(async function should_handle_async_getters() {
  const context = {get FOO() { return Promise.resolve(true); }};
  is(await ASRouterTargeting.isMatch("FOO", {}, context), true, "should return true for a matching async value");
});

// ASRouterTargeting.findMatchingMessage
add_task(async function find_matching_message() {
  const messages = [
    {id: "foo", targeting: "FOO"},
    {id: "bar", targeting: "!FOO"}
  ];
  const context = {FOO: true};

  const match = await ASRouterTargeting.findMatchingMessage({messages, target: {}, context});

  is(match, messages[0], "should match and return the correct message");
});

add_task(async function return_nothing_for_no_matching_message() {
  const messages = [{id: "bar", targeting: "!FOO"}];
  const context = {FOO: true};

  const match = await ASRouterTargeting.findMatchingMessage({messages, target: {}, context});

  is(match, undefined, "should return nothing since no matching message exists");
});

// ASRouterTargeting.Environment
add_task(async function checkProfileAgeCreated() {
  let profileAccessor = new ProfileAge();
  is(await ASRouterTargeting.Environment.profileAgeCreated, await profileAccessor.created,
    "should return correct profile age creation date");

  const message = {id: "foo", targeting: `profileAgeCreated > ${await profileAccessor.created - 100}`};
  is(await ASRouterTargeting.findMatchingMessage({messages: [message], target: {}}), message,
    "should select correct item by profile age created");
});

add_task(async function checkProfileAgeReset() {
  let profileAccessor = new ProfileAge();
  is(await ASRouterTargeting.Environment.profileAgeReset, await profileAccessor.reset,
    "should return correct profile age reset");

  const message = {id: "foo", targeting: `profileAgeReset == ${await profileAccessor.reset}`};
  is(await ASRouterTargeting.findMatchingMessage({messages: [message], target: {}}), message,
    "should select correct item by profile age reset");
});

add_task(async function checkhasFxAccount() {
  await pushPrefs(["services.sync.username", "someone@foo.com"]);
  is(await ASRouterTargeting.Environment.hasFxAccount, true,
    "should return true if a fx account is set");

  const message = {id: "foo", targeting: "hasFxAccount"};
  is(await ASRouterTargeting.findMatchingMessage({messages: [message], target: {}}), message,
    "should select correct item by hasFxAccount");
});

add_task(async function checksearchEngines() {
  const result = await ASRouterTargeting.Environment.searchEngines;
  const expectedInstalled = Services.search.getVisibleEngines()
    .map(engine => engine.identifier)
    .sort()
    .join(",");
  ok(result.installed.length,
    "searchEngines.installed should be a non-empty array");
  is(result.installed.sort().join(","), expectedInstalled,
    "searchEngines.installed should be an array of visible search engines");
  ok(result.current && typeof result.current === "string",
    "searchEngines.current should be a truthy string");
  is(result.current, Services.search.currentEngine.identifier,
    "searchEngines.current should be the current engine name");

  const message = {id: "foo", targeting: `searchEngines[.current == ${Services.search.currentEngine.identifier}]`};
  is(await ASRouterTargeting.findMatchingMessage({messages: [message], target: {}}), message,
    "should select correct item by searchEngines.current");

  const message2 = {id: "foo", targeting: `searchEngines[${Services.search.getVisibleEngines()[0].identifier} in .installed]`};
  is(await ASRouterTargeting.findMatchingMessage({messages: [message2], target: {}}), message2,
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
  is(await ASRouterTargeting.findMatchingMessage({messages: [message], target: {}}), message,
    "should select correct item by isDefaultBrowser");
});

add_task(async function checkdevToolsOpenedCount() {
  await pushPrefs(["devtools.selfxss.count", 5]);
  is(ASRouterTargeting.Environment.devToolsOpenedCount, 5,
    "devToolsOpenedCount should be equal to devtools.selfxss.count pref value");
  const message = {id: "foo", targeting: "devToolsOpenedCount >= 5"};
  is(await ASRouterTargeting.findMatchingMessage({messages: [message], target: {}}), message,
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
      version: FAKE_VERSION
    }
  });

  await Promise.all([
    AddonTestUtils.promiseWebExtensionStartup(FAKE_ID),
    AddonManager.installTemporaryAddon(xpi)
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
    ["https://mozilla3.com/", 1, timeDaysAgo(2)]   // frecency 100
  ]) {
    [...Array(count).keys()].forEach(() => visits.push({
      uri,
      visitDate: visitDate * 1000 // Places expects microseconds
    }));
  }

  await PlacesTestUtils.addVisits(visits);

  let message = {id: "foo", targeting: "'mozilla3.com' in topFrecentSites|mapToProperty('host')"};
  is(await ASRouterTargeting.findMatchingMessage({messages: [message], target: {}}), message,
    "should select correct item by host in topFrecentSites");

  message = {id: "foo", targeting: "'non-existent.com' in topFrecentSites|mapToProperty('host')"};
  is(await ASRouterTargeting.findMatchingMessage({messages: [message], target: {}}), undefined,
    "should not select incorrect item by host in topFrecentSites");

  message = {id: "foo", targeting: "'mozilla2.com' in topFrecentSites[.frecency >= 400]|mapToProperty('host')"};
  is(await ASRouterTargeting.findMatchingMessage({messages: [message], target: {}}), message,
    "should select correct item when filtering by frecency");

  message = {id: "foo", targeting: "'mozilla2.com' in topFrecentSites[.frecency >= 600]|mapToProperty('host')"};
  is(await ASRouterTargeting.findMatchingMessage({messages: [message], target: {}}), undefined,
    "should not select incorrect item when filtering by frecency");

  message = {id: "foo", targeting: `'mozilla2.com' in topFrecentSites[.lastVisitDate >= ${timeDaysAgo(1) - 1}]|mapToProperty('host')`};
  is(await ASRouterTargeting.findMatchingMessage({messages: [message], target: {}}), message,
    "should select correct item when filtering by lastVisitDate");

  message = {id: "foo", targeting: `'mozilla2.com' in topFrecentSites[.lastVisitDate >= ${timeDaysAgo(0) - 1}]|mapToProperty('host')`};
  is(await ASRouterTargeting.findMatchingMessage({messages: [message], target: {}}), undefined,
    "should not select incorrect item when filtering by lastVisitDate");

  message = {id: "foo", targeting: `(topFrecentSites[.frecency >= 900 && .lastVisitDate >= ${timeDaysAgo(1) - 1}]|mapToProperty('host') intersect ['mozilla3.com', 'mozilla2.com', 'mozilla1.com'])|length > 0`};
  is(await ASRouterTargeting.findMatchingMessage({messages: [message], target: {}}), message,
    "should select correct item when filtering by frecency and lastVisitDate with multiple candidate domains");
});
