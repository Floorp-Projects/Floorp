/**
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

async function require_module(id) {
  if (!require_module.moduleLoader) {
    const { ModuleLoader } = ChromeUtils.import(
      "resource://testing-common/dom/quota/test/modules/ModuleLoader.jsm"
    );

    const base = Services.io.newFileURI(do_get_file("")).spec;

    const depth = "../../../../";

    Cu.importGlobalProperties(["storage"]);

    const proto = {
      Assert,
      Cr,
      navigator: {
        storage,
      },
    };

    require_module.moduleLoader = new ModuleLoader(base, depth, proto);
  }

  return require_module.moduleLoader.require(id);
}

async function run_test_in_worker(script) {
  const { runTestInWorker } = ChromeUtils.import(
    "resource://testing-common/dom/quota/test/modules/WorkerDriver.jsm"
  );

  const base = "resource://testing-common/dom/fs/test/xpcshell/";

  const listener = {
    onOk(value, message) {
      ok(value, message);
    },
    onIs(a, b, message) {
      Assert.equal(a, b, message);
    },
    onInfo(message) {
      info(message);
    },
  };

  await runTestInWorker(script, base, listener);
}

add_setup(async function() {
  const {
    setStoragePrefs,
    clearStoragePrefs,
    clearStoragesForOrigin,
  } = ChromeUtils.import(
    "resource://testing-common/dom/quota/test/modules/StorageUtils.jsm"
  );

  const optionalPrefsToSet = [["dom.fs.enabled", true]];

  setStoragePrefs(optionalPrefsToSet);

  registerCleanupFunction(async function() {
    const principal = Cc["@mozilla.org/systemprincipal;1"].createInstance(
      Ci.nsIPrincipal
    );

    await clearStoragesForOrigin(principal);

    const optionalPrefsToClear = ["dom.fs.enabled"];

    clearStoragePrefs(optionalPrefsToClear);
  });

  do_get_profile();
});
