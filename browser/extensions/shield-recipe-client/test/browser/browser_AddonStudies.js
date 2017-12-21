"use strict";

Cu.import("resource://gre/modules/IndexedDB.jsm", this);
Cu.import("resource://testing-common/TestUtils.jsm", this);
Cu.import("resource://testing-common/AddonTestUtils.jsm", this);
Cu.import("resource://shield-recipe-client/lib/Addons.jsm", this);
Cu.import("resource://shield-recipe-client/lib/AddonStudies.jsm", this);

// Initialize test utils
AddonTestUtils.initMochitest(this);

let _startArgsFactoryId = 1;
function startArgsFactory(args) {
  return Object.assign({
    recipeId: _startArgsFactoryId++,
    name: "Test",
    description: "Test",
    addonUrl: "http://test/addon.xpi",
  }, args);
}

decorate_task(
  AddonStudies.withStudies(),
  async function testGetMissing() {
    is(
      await AddonStudies.get("does-not-exist"),
      null,
      "get returns null when the requested study does not exist"
    );
  }
);

decorate_task(
  AddonStudies.withStudies([
    studyFactory({name: "test-study"}),
  ]),
  async function testGet([study]) {
    const storedStudy = await AddonStudies.get(study.recipeId);
    Assert.deepEqual(study, storedStudy, "get retrieved a study from storage.");
  }
);

decorate_task(
  AddonStudies.withStudies([
    studyFactory(),
    studyFactory(),
  ]),
  async function testGetAll(studies) {
    const storedStudies = await AddonStudies.getAll();
    Assert.deepEqual(
      new Set(storedStudies),
      new Set(studies),
      "getAll returns every stored study.",
    );
  }
);

decorate_task(
  AddonStudies.withStudies([
    studyFactory({name: "test-study"}),
  ]),
  async function testHas([study]) {
    let hasStudy = await AddonStudies.has(study.recipeId);
    ok(hasStudy, "has returns true for a study that exists in storage.");

    hasStudy = await AddonStudies.has("does-not-exist");
    ok(!hasStudy, "has returns false for a study that doesn't exist in storage.");
  }
);

decorate_task(
  AddonStudies.withStudies(),
  async function testCloseDatabase() {
    await AddonStudies.close();
    const openSpy = sinon.spy(IndexedDB, "open");
    sinon.assert.notCalled(openSpy);

    // Using studies at all should open the database, but only once.
    await AddonStudies.has("foo");
    await AddonStudies.get("foo");
    sinon.assert.calledOnce(openSpy);

    // close can be called multiple times
    await AddonStudies.close();
    await AddonStudies.close();

    // After being closed, new operations cause the database to be opened again
    await AddonStudies.has("test-study");
    sinon.assert.calledTwice(openSpy);

    openSpy.restore();
  }
);

decorate_task(
  AddonStudies.withStudies([
    studyFactory({name: "test-study1"}),
    studyFactory({name: "test-study2"}),
  ]),
  async function testClear([study1, study2]) {
    const hasAll = (
      (await AddonStudies.has(study1.recipeId)) &&
      (await AddonStudies.has(study2.recipeId))
    );
    ok(hasAll, "Before calling clear, both studies are in storage.");

    await AddonStudies.clear();
    const hasAny = (
      (await AddonStudies.has(study1.recipeId)) ||
      (await AddonStudies.has(study2.recipeId))
    );
    ok(!hasAny, "After calling clear, all studies are removed from storage.");
  }
);

add_task(async function testStartRequiredArguments() {
  const requiredArguments = startArgsFactory();
  for (const key in requiredArguments) {
    const args = Object.assign({}, requiredArguments);
    delete args[key];
    Assert.rejects(
      AddonStudies.start(args),
      /Required arguments/,
      `start rejects when missing required argument ${key}.`
    );
  }
});

decorate_task(
  AddonStudies.withStudies([
    studyFactory(),
  ]),
  async function testStartExisting([study]) {
    Assert.rejects(
      AddonStudies.start(startArgsFactory({recipeId: study.recipeId})),
      /already exists/,
      "start rejects when a study exists with the given recipeId already."
    );
  }
);

decorate_task(
  withStub(Addons, "applyInstall"),
  withWebExtension(),
  async function testStartAddonCleanup(applyInstallStub, [addonId, addonFile]) {
    applyInstallStub.rejects(new Error("Fake failure"));

    const addonUrl = Services.io.newFileURI(addonFile).spec;
    await Assert.rejects(
      AddonStudies.start(startArgsFactory({addonUrl})),
      /Fake failure/,
      "start rejects when the Addons.applyInstall function rejects"
    );

    const addon = await Addons.get(addonId);
    ok(!addon, "If something fails during start after the add-on is installed, it is uninstalled.");
  }
);

const testOverwriteId = "testStartAddonNoOverwrite@example.com";
decorate_task(
  withInstalledWebExtension({version: "1.0", id: testOverwriteId}),
  withWebExtension({version: "2.0", id: testOverwriteId}),
  async function testStartAddonNoOverwrite([installedId, installedFile], [id, addonFile]) {
    const addonUrl = Services.io.newFileURI(addonFile).spec;
    await Assert.rejects(
      AddonStudies.start(startArgsFactory({addonUrl})),
      /updating is disabled/,
      "start rejects when the study add-on is already installed"
    );

    await Addons.uninstall(testOverwriteId);
  }
);

decorate_task(
  withWebExtension({version: "2.0"}),
  AddonStudies.withStudies(),
  async function testStart([addonId, addonFile]) {
    const startupPromise = AddonTestUtils.promiseWebExtensionStartup(addonId);
    const addonUrl = Services.io.newFileURI(addonFile).spec;

    let addon = await Addons.get(addonId);
    is(addon, null, "Before start is called, the add-on is not installed.");

    const args = startArgsFactory({
      name: "Test Study",
      description: "Test Desc",
      addonUrl,
    });
    await AddonStudies.start(args);
    await startupPromise;

    addon = await Addons.get(addonId);
    ok(addon, "After start is called, the add-on is installed.");

    const study = await AddonStudies.get(args.recipeId);
    Assert.deepEqual(
      study,
      {
        recipeId: args.recipeId,
        name: args.name,
        description: args.description,
        addonId,
        addonVersion: "2.0",
        addonUrl,
        active: true,
        studyStartDate: study.studyStartDate,
      },
      "start saves study data to storage",
    );
    ok(study.studyStartDate, "start assigns a value to the study start date.");

    await AddonStudies.stop(args.recipeId);
  }
);

decorate_task(
  AddonStudies.withStudies(),
  async function testStopNoStudy() {
    await Assert.rejects(
      AddonStudies.stop("does-not-exist"),
      /No study found/,
      "stop rejects when no study exists for the given recipe."
    );
  }
);

decorate_task(
  AddonStudies.withStudies([
    studyFactory({active: false}),
  ]),
  async function testStopInactiveStudy([study]) {
    await Assert.rejects(
      AddonStudies.stop(study.recipeId),
      /already inactive/,
      "stop rejects when the requested study is already inactive."
    );
  }
);

const testStopId = "testStop@example.com";
decorate_task(
  AddonStudies.withStudies([
    studyFactory({active: true, addonId: testStopId, studyEndDate: null}),
  ]),
  withInstalledWebExtension({id: testStopId}),
  async function testStop([study], [addonId, addonFile]) {
    await AddonStudies.stop(study.recipeId);
    const newStudy = await AddonStudies.get(study.recipeId);
    ok(!newStudy.active, "stop marks the study as inactive.");
    ok(newStudy.studyEndDate, "stop saves the study end date.");

    const addon = await Addons.get(addonId);
    is(addon, null, "stop uninstalls the study add-on.");
  }
);

decorate_task(
  AddonStudies.withStudies([
    studyFactory({active: true, addonId: "testStopWarn@example.com", studyEndDate: null}),
  ]),
  async function testStopWarn([study]) {
    const addon = await Addons.get("testStopWarn@example.com");
    is(addon, null, "Before start is called, the add-on is not installed.");

    // If the add-on is not installed, log a warning to the console, but do not
    // throw.
    await new Promise(resolve => {
      SimpleTest.waitForExplicitFinish();
      SimpleTest.monitorConsole(resolve, [{message: /Could not uninstall addon/}]);
      AddonStudies.stop(study.recipeId).then(() => SimpleTest.endMonitorConsole());
    });
  }
);

decorate_task(
  AddonStudies.withStudies([
    studyFactory({active: true, addonId: "does.not.exist@example.com", studyEndDate: null}),
    studyFactory({active: true, addonId: "installed@example.com"}),
    studyFactory({active: false, addonId: "already.gone@example.com", studyEndDate: new Date(2012, 1)}),
  ]),
  withInstalledWebExtension({id: "installed@example.com"}),
  async function testInit([activeStudy, activeInstalledStudy, inactiveStudy]) {
    await AddonStudies.init();

    const newActiveStudy = await AddonStudies.get(activeStudy.recipeId);
    ok(!newActiveStudy.active, "init marks studies as inactive if their add-on is not installed.");
    ok(
      newActiveStudy.studyEndDate,
      "init sets the study end date if a study's add-on is not installed."
    );

    const newInactiveStudy = await AddonStudies.get(inactiveStudy.recipeId);
    is(
      newInactiveStudy.studyEndDate.getFullYear(),
      2012,
      "init does not modify inactive studies."
    );

    const newActiveInstalledStudy = await AddonStudies.get(activeInstalledStudy.recipeId);
    Assert.deepEqual(
      activeInstalledStudy,
      newActiveInstalledStudy,
      "init does not modify studies whose add-on is still installed."
    );
  }
);

decorate_task(
  AddonStudies.withStudies([
    studyFactory({active: true, addonId: "installed@example.com", studyEndDate: null}),
  ]),
  withInstalledWebExtension({id: "installed@example.com"}),
  async function testInit([study], [id, addonFile]) {
    await Addons.uninstall(id);
    await TestUtils.topicObserved("shield-study-ended");

    const newStudy = await AddonStudies.get(study.recipeId);
    ok(!newStudy.active, "Studies are marked as inactive when their add-on is uninstalled.");
    ok(
      newStudy.studyEndDate,
      "The study end date is set when the add-on for the study is uninstalled."
    );
  }
);
