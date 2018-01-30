"use strict";

Cu.import("resource://gre/modules/AppConstants.jsm");
Cu.import("resource://testing-common/AddonTestUtils.jsm", this);
Cu.import("resource://shield-recipe-client/lib/AddonStudies.jsm", this);
Cu.import("resource://shield-recipe-client/lib/NormandyDriver.jsm", this);
Cu.import("resource://shield-recipe-client/lib/PreferenceExperiments.jsm", this);

add_task(withDriver(Assert, async function uuids(driver) {
  // Test that it is a UUID
  const uuid1 = driver.uuid();
  ok(UUID_REGEX.test(uuid1), "valid uuid format");

  // Test that UUIDs are different each time
  const uuid2 = driver.uuid();
  isnot(uuid1, uuid2, "uuids are unique");
}));

add_task(withDriver(Assert, async function installXpi(driver) {
  // Test that we can install an XPI from any URL
  // Create before install so that the listener is added before startup completes.
  const startupPromise = AddonTestUtils.promiseWebExtensionStartup("normandydriver@example.com");

  var addonId = await driver.addons.install(TEST_XPI_URL);
  is(addonId, "normandydriver@example.com", "Expected test addon was installed");
  isnot(addonId, null, "Addon install was successful");

  // Wait until the add-on is fully started up to uninstall it.
  await startupPromise;

  const uninstallMsg = await driver.addons.uninstall(addonId);
  is(uninstallMsg, null, `Uninstall returned an unexpected message [${uninstallMsg}]`);
}));

add_task(withDriver(Assert, async function uninstallInvalidAddonId(driver) {
  const invalidAddonId = "not_a_valid_xpi_id@foo.bar";
  try {
    await driver.addons.uninstall(invalidAddonId);
    ok(false, `Uninstalling an invalid XPI should fail. addons.uninstall resolved successfully though.`);
  } catch (e) {
    ok(true, `This is the expected failure`);
  }
}));


add_task(withDriver(Assert, async function installXpiBadURL(driver) {
  let xpiUrl;
  if (AppConstants.platform === "win") {
    xpiUrl = "file:///C:/invalid_xpi.xpi";
  } else {
    xpiUrl = "file:///tmp/invalid_xpi.xpi";
  }

  try {
    await driver.addons.install(xpiUrl);
    ok(false, "Installation succeeded on an XPI that doesn't exist");
  } catch (reason) {
    ok(true, `Installation was rejected: [${reason}]`);
  }
}));

add_task(withDriver(Assert, async function userId(driver) {
  // Test that userId is a UUID
  ok(UUID_REGEX.test(driver.userId), "userId is a uuid");
}));

add_task(withDriver(Assert, async function syncDeviceCounts(driver) {
  let client = await driver.client();
  is(client.syncMobileDevices, 0, "syncMobileDevices defaults to zero");
  is(client.syncDesktopDevices, 0, "syncDesktopDevices defaults to zero");
  is(client.syncTotalDevices, 0, "syncTotalDevices defaults to zero");

  await SpecialPowers.pushPrefEnv({
    set: [
      ["services.sync.clients.devices.mobile", 5],
      ["services.sync.clients.devices.desktop", 4],
    ],
  });

  client = await driver.client();
  is(client.syncMobileDevices, 5, "syncMobileDevices is read when set");
  is(client.syncDesktopDevices, 4, "syncDesktopDevices is read when set");
  is(client.syncTotalDevices, 9, "syncTotalDevices is read when set");
}));

add_task(withDriver(Assert, async function distribution(driver) {
  let client = await driver.client();
  is(client.distribution, "default", "distribution has a default value");

  await SpecialPowers.pushPrefEnv({set: [["distribution.id", "funnelcake"]]});
  client = await driver.client();
  is(client.distribution, "funnelcake", "distribution is read from preferences");
}));

decorate_task(
  withSandboxManager(Assert),
  async function testCreateStorage(sandboxManager) {
    const driver = new NormandyDriver(sandboxManager);
    sandboxManager.cloneIntoGlobal("driver", driver, {cloneFunctions: true});

    // Assertion helpers
    sandboxManager.addGlobal("is", is);
    sandboxManager.addGlobal("deepEqual", (...args) => Assert.deepEqual(...args));

    await sandboxManager.evalInSandbox(`
      (async function sandboxTest() {
        const store = driver.createStorage("testprefix");
        const otherStore = driver.createStorage("othertestprefix");
        await store.clear();
        await otherStore.clear();

        await store.setItem("willremove", 7);
        await otherStore.setItem("willremove", 4);
        is(await store.getItem("willremove"), 7, "createStorage stores sandbox values");
        is(
          await otherStore.getItem("willremove"),
          4,
          "values are not shared between createStorage stores",
        );

        const deepValue = {"foo": ["bar", "baz"]};
        await store.setItem("deepValue", deepValue);
        deepEqual(await store.getItem("deepValue"), deepValue, "createStorage clones stored values");

        await store.removeItem("willremove");
        is(await store.getItem("willremove"), null, "createStorage removes items");

        is('prefix' in store, false, "createStorage doesn't expose non-whitelist attributes");
      })();
    `);
  }
);

add_task(withDriver(Assert, async function getAddon(driver, sandboxManager) {
  const ADDON_ID = "normandydriver@example.com";
  let addon = await driver.addons.get(ADDON_ID);
  Assert.equal(addon, null, "Add-on is not yet installed");

  await driver.addons.install(TEST_XPI_URL);
  addon = await driver.addons.get(ADDON_ID);

  Assert.notEqual(addon, null, "Add-on object was returned");
  ok(addon.installDate instanceof sandboxManager.sandbox.Date, "installDate should be a Date object");

  Assert.deepEqual(addon, {
    id: "normandydriver@example.com",
    name: "normandy_fixture",
    version: "1.0",
    installDate: addon.installDate,
    isActive: true,
    type: "extension",
  }, "Add-on is installed");

  await driver.addons.uninstall(ADDON_ID);
  addon = await driver.addons.get(ADDON_ID);

  Assert.equal(addon, null, "Add-on has been uninstalled");
}));

decorate_task(
  withSandboxManager(Assert),
  async function testAddonsGetWorksInSandbox(sandboxManager) {
    const driver = new NormandyDriver(sandboxManager);
    sandboxManager.cloneIntoGlobal("driver", driver, {cloneFunctions: true});

    // Assertion helpers
    sandboxManager.addGlobal("is", is);
    sandboxManager.addGlobal("deepEqual", (...args) => Assert.deepEqual(...args));

    const ADDON_ID = "normandydriver@example.com";

    await driver.addons.install(TEST_XPI_URL);

    await sandboxManager.evalInSandbox(`
      (async function sandboxTest() {
        const addon = await driver.addons.get("${ADDON_ID}");

        deepEqual(addon, {
          id: "${ADDON_ID}",
          name: "normandy_fixture",
          version: "1.0",
          installDate: addon.installDate,
          isActive: true,
          type: "extension",
        }, "Add-on is accesible in the driver");
      })();
    `);

    await driver.addons.uninstall(ADDON_ID);
  }
);

decorate_task(
  withSandboxManager(Assert),
  withWebExtension({id: "driver-addon-studies@example.com"}),
  AddonStudies.withStudies(),
  async function testAddonStudies(sandboxManager, [addonId, addonFile]) {
    const addonUrl = Services.io.newFileURI(addonFile).spec;
    const driver = new NormandyDriver(sandboxManager);
    sandboxManager.cloneIntoGlobal("driver", driver, {cloneFunctions: true});

    // Assertion helpers
    sandboxManager.addGlobal("is", is);
    sandboxManager.addGlobal("ok", ok);

    await sandboxManager.evalInSandbox(`
      (async function sandboxTest() {
        const recipeId = 5;
        let hasStudy = await driver.studies.has(recipeId);
        ok(!hasStudy, "studies.has returns false if the study hasn't been started yet.");

        await driver.studies.start({
          recipeId,
          name: "fake",
          description: "fake",
          addonUrl: "${addonUrl}",
        });
        hasStudy = await driver.studies.has(recipeId);
        ok(hasStudy, "studies.has returns true after the study has been started.");

        let study = await driver.studies.get(recipeId);
        is(
          study.addonId,
          "driver-addon-studies@example.com",
          "studies.get fetches studies from within a sandbox."
        );
        ok(study.active, "Studies are marked as active after being started by the driver.");

        await driver.studies.stop(recipeId);
        study = await driver.studies.get(recipeId);
        ok(!study.active, "Studies are marked as inactive after being stopped by the driver.");
      })();
    `);
  }
);

decorate_task(
  withPrefEnv({
    set: [
      ["test.char", "a string"],
      ["test.int", 5],
      ["test.bool", true],
    ],
  }),
  withSandboxManager(Assert, async function testPreferences(sandboxManager) {
    const driver = new NormandyDriver(sandboxManager);
    sandboxManager.cloneIntoGlobal("driver", driver, {cloneFunctions: true});

    // Assertion helpers
    sandboxManager.addGlobal("is", is);
    sandboxManager.addGlobal("ok", ok);
    sandboxManager.addGlobal("assertThrows", Assert.throws.bind(Assert));

    await sandboxManager.evalInSandbox(`
      (async function sandboxTest() {
        ok(
          driver.preferences.getBool("test.bool"),
          "preferences.getBool can retrieve boolean preferences."
        );
        is(
          driver.preferences.getInt("test.int"),
          5,
          "preferences.getInt can retrieve integer preferences."
        );
        is(
          driver.preferences.getChar("test.char"),
          "a string",
          "preferences.getChar can retrieve string preferences."
        );
        assertThrows(
          () => driver.preferences.getChar("test.int"),
          "preferences.getChar throws when retreiving a non-string preference."
        );
        assertThrows(
          () => driver.preferences.getInt("test.bool"),
          "preferences.getInt throws when retreiving a non-integer preference."
        );
        assertThrows(
          () => driver.preferences.getBool("test.char"),
          "preferences.getBool throws when retreiving a non-boolean preference."
        );
        assertThrows(
          () => driver.preferences.getChar("test.does.not.exist"),
          "preferences.getChar throws when retreiving a non-existant preference."
        );
        assertThrows(
          () => driver.preferences.getInt("test.does.not.exist"),
          "preferences.getInt throws when retreiving a non-existant preference."
        );
        assertThrows(
          () => driver.preferences.getBool("test.does.not.exist"),
          "preferences.getBool throws when retreiving a non-existant preference."
        );
        ok(
          driver.preferences.getBool("test.does.not.exist", true),
          "preferences.getBool returns a default value if the preference doesn't exist."
        );
        is(
          driver.preferences.getInt("test.does.not.exist", 7),
          7,
          "preferences.getInt returns a default value if the preference doesn't exist."
        );
        is(
          driver.preferences.getChar("test.does.not.exist", "default"),
          "default",
          "preferences.getChar returns a default value if the preference doesn't exist."
        );
        ok(
          driver.preferences.has("test.char"),
          "preferences.has returns true if the given preference exists."
        );
        ok(
          !driver.preferences.has("test.does.not.exist"),
          "preferences.has returns false if the given preference does not exist."
        );
      })();
    `);
  })
);

decorate_task(
  withSandboxManager(Assert),
  withMockPreferences,
  PreferenceExperiments.withMockExperiments,
  async function testPreferenceStudies(sandboxManager) {
    const driver = new NormandyDriver(sandboxManager);
    sandboxManager.cloneIntoGlobal("driver", driver, {cloneFunctions: true});

    // Assertion helpers
    sandboxManager.addGlobal("is", is);
    sandboxManager.addGlobal("ok", ok);

    await sandboxManager.evalInSandbox(`
      (async function sandboxTest() {
        const studyName = "preftest";
        let hasStudy = await driver.preferenceExperiments.has(studyName);
        ok(!hasStudy, "preferenceExperiments.has returns false if the study hasn't been started yet.");

        await driver.preferenceExperiments.start({
          name: studyName,
          branch: "control",
          preferenceName: "test.pref",
          preferenceValue: true,
          preferenceBranchType: "user",
          preferenceType: "boolean",
        });
        hasStudy = await driver.preferenceExperiments.has(studyName);
        ok(hasStudy, "preferenceExperiments.has returns true after the study has been started.");

        let study = await driver.preferenceExperiments.get(studyName);
        is(
          study.branch,
          "control",
          "preferenceExperiments.get fetches studies from within a sandbox."
        );
        ok(!study.expired, "Studies are marked as active after being started by the driver.");

        await driver.preferenceExperiments.stop(studyName);
        study = await driver.preferenceExperiments.get(studyName);
        ok(study.expired, "Studies are marked as inactive after being stopped by the driver.");
      })();
    `);
  }
);
