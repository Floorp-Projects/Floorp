ChromeUtils.defineModuleGetter(this, "ASRouterTargeting",
"resource://activity-stream/lib/ASRouterTargeting.jsm");
ChromeUtils.defineModuleGetter(this, "ProfileAge",
  "resource://gre/modules/ProfileAge.jsm");
ChromeUtils.defineModuleGetter(this, "AddonManager",
  "resource://gre/modules/AddonManager.jsm");

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

  const match = await ASRouterTargeting.findMatchingMessage(messages, {}, context);

  is(match, messages[0], "should match and return the correct message");
});

add_task(async function return_nothing_for_no_matching_message() {
  const messages = [{id: "bar", targeting: "!FOO"}];
  const context = {FOO: true};

  const match = await ASRouterTargeting.findMatchingMessage(messages, {}, context);

  is(match, undefined, "should return nothing since no matching message exists");
});

// ASRouterTargeting.Environment
add_task(async function checkProfileAgeCreated() {
  let profileAccessor = new ProfileAge();
  is(await ASRouterTargeting.Environment.profileAgeCreated, await profileAccessor.created,
    "should return correct profile age creation date");

  const message = {id: "foo", targeting: `profileAgeCreated > ${await profileAccessor.created - 100}`};
  is(await ASRouterTargeting.findMatchingMessage([message], {}), message,
    "should select correct item by profile age created");
});

add_task(async function checkProfileAgeReset() {
  let profileAccessor = new ProfileAge();
  is(await ASRouterTargeting.Environment.profileAgeReset, await profileAccessor.reset,
    "should return correct profile age reset");

  const message = {id: "foo", targeting: `profileAgeReset == ${await profileAccessor.reset}`};
  is(await ASRouterTargeting.findMatchingMessage([message], {}), message,
    "should select correct item by profile age reset");
});

add_task(async function checkhasFxAccount() {
  await pushPrefs(["services.sync.username", "someone@foo.com"]);
  is(await ASRouterTargeting.Environment.hasFxAccount, true,
    "should return true if a fx account is set");

  const message = {id: "foo", targeting: "hasFxAccount"};
  is(await ASRouterTargeting.findMatchingMessage([message], {}), message,
    "should select correct item by hasFxAccount");
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
    (Math.abs(new Date() - new Date(testAddon.installDate)) < 60 * 1000),
    "should correctly provide `installDate` property from full data");
});
