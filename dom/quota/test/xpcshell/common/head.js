/**
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

const NS_OK = Cr.NS_OK;
const NS_ERROR_FAILURE = Cr.NS_ERROR_FAILURE;
const NS_ERROR_UNEXPECTED = Cr.NS_ERROR_UNEXPECTED;
const NS_ERROR_FILE_NO_DEVICE_SPACE = Cr.NS_ERROR_FILE_NO_DEVICE_SPACE;

const loggingEnabled = false;

var testGenerator;

loadScript("dom/quota/test/common/xpcshell.js");

function log(msg) {
  if (loggingEnabled) {
    info(msg);
  }
}

function is(a, b, msg) {
  Assert.equal(a, b, msg);
}

function ok(cond, msg) {
  Assert.ok(!!cond, msg);
}

function todo(cond, msg) {
  todo_check_true(cond);
}

function run_test() {
  runTest();
}

if (!this.runTest) {
  this.runTest = function () {
    do_get_profile();

    enableStorageTesting();
    enableTesting();

    // In order to support converting tests to using async functions from using
    // generator functions, we detect async functions by checking the name of
    // function's constructor.
    Assert.ok(
      typeof testSteps === "function",
      "There should be a testSteps function"
    );
    if (testSteps.constructor.name === "AsyncFunction") {
      // Do run our existing cleanup function that would normally be called by
      // the generator's call to finishTest().
      registerCleanupFunction(function () {
        resetStorageTesting();
        resetTesting();
      });

      add_task(testSteps);

      // Since we defined run_test, we must invoke run_next_test() to start the
      // async test.
      run_next_test();
    } else {
      Assert.ok(
        testSteps.constructor.name === "GeneratorFunction",
        "Unsupported function type"
      );

      do_test_pending();

      testGenerator = testSteps();
      testGenerator.next();
    }
  };
}

function finishTest() {
  resetStorageTesting();
  resetTesting();

  executeSoon(function () {
    do_test_finished();
  });
}

function grabArgAndContinueHandler(arg) {
  testGenerator.next(arg);
}

function continueToNextStep() {
  executeSoon(function () {
    testGenerator.next();
  });
}

function continueToNextStepSync() {
  testGenerator.next();
}

function enableTesting() {
  SpecialPowers.setBoolPref(
    "dom.storage.enable_unsupported_legacy_implementation",
    false
  );
}

function resetTesting() {
  SpecialPowers.clearUserPref(
    "dom.storage.enable_unsupported_legacy_implementation"
  );
}

function setGlobalLimit(globalLimit) {
  SpecialPowers.setIntPref(
    "dom.quotaManager.temporaryStorage.fixedLimit",
    globalLimit
  );
}

function resetGlobalLimit() {
  SpecialPowers.clearUserPref("dom.quotaManager.temporaryStorage.fixedLimit");
}

function storageInitialized(callback) {
  let request = SpecialPowers._getQuotaManager().storageInitialized();
  request.callback = callback;

  return request;
}

function temporaryStorageInitialized(callback) {
  let request = SpecialPowers._getQuotaManager().temporaryStorageInitialized();
  request.callback = callback;

  return request;
}

function init(callback) {
  let request = SpecialPowers._getQuotaManager().init();
  request.callback = callback;

  return request;
}

function initTemporaryStorage(callback) {
  let request = SpecialPowers._getQuotaManager().initTemporaryStorage();
  request.callback = callback;

  return request;
}

function initPersistentOrigin(principal, callback) {
  let request =
    SpecialPowers._getQuotaManager().initializePersistentOrigin(principal);
  request.callback = callback;

  return request;
}

function initTemporaryOrigin(persistence, principal, callback) {
  let request = SpecialPowers._getQuotaManager().initializeTemporaryOrigin(
    persistence,
    principal
  );
  request.callback = callback;

  return request;
}

function initPersistentClient(principal, client, callback) {
  let request = SpecialPowers._getQuotaManager().initializePersistentClient(
    principal,
    client
  );
  request.callback = callback;

  return request;
}

function initTemporaryClient(persistence, principal, client, callback) {
  let request = SpecialPowers._getQuotaManager().initializeTemporaryClient(
    persistence,
    principal,
    client
  );
  request.callback = callback;

  return request;
}

function getFullOriginMetadata(persistence, principal, callback) {
  const request = SpecialPowers._getQuotaManager().getFullOriginMetadata(
    persistence,
    principal
  );
  request.callback = callback;

  return request;
}

function clearClient(principal, persistence, client, callback) {
  let request = SpecialPowers._getQuotaManager().clearStoragesForPrincipal(
    principal,
    persistence,
    client
  );
  request.callback = callback;

  return request;
}

function clearOrigin(principal, persistence, callback) {
  let request = SpecialPowers._getQuotaManager().clearStoragesForPrincipal(
    principal,
    persistence
  );
  request.callback = callback;

  return request;
}

function clearOriginsByPrefix(principal, persistence, callback) {
  let request = SpecialPowers._getQuotaManager().clearStoragesForOriginPrefix(
    principal,
    persistence
  );
  request.callback = callback;

  return request;
}

function clearPrivateBrowsing(callback) {
  let request =
    SpecialPowers._getQuotaManager().clearStoragesForPrivateBrowsing();
  request.callback = callback;

  return request;
}

function resetClient(principal, client) {
  let request = Services.qms.resetStoragesForPrincipal(
    principal,
    "default",
    client
  );

  return request;
}

function persist(principal, callback) {
  let request = SpecialPowers._getQuotaManager().persist(principal);
  request.callback = callback;

  return request;
}

function persisted(principal, callback) {
  let request = SpecialPowers._getQuotaManager().persisted(principal);
  request.callback = callback;

  return request;
}

function estimateOrigin(principal, callback) {
  let request = SpecialPowers._getQuotaManager().estimate(principal);
  request.callback = callback;

  return request;
}

function listOrigins(callback) {
  let request = SpecialPowers._getQuotaManager().listOrigins(callback);
  request.callback = callback;

  return request;
}

function getPersistedFromMetadata(readBuffer) {
  const persistedPosition = 8; // Persisted state is stored in the 9th byte
  let view =
    readBuffer instanceof Uint8Array ? readBuffer : new Uint8Array(readBuffer);

  return !!view[persistedPosition];
}

function grabResultAndContinueHandler(request) {
  testGenerator.next(request.result);
}

function grabUsageAndContinueHandler(request) {
  testGenerator.next(request.result.usage);
}

function getUsage(usageHandler, getAll) {
  let request = SpecialPowers._getQuotaManager().getUsage(usageHandler, getAll);

  return request;
}

function getOriginUsage(principal, fromMemory = false) {
  let request = Services.qms.getUsageForPrincipal(
    principal,
    function () {},
    fromMemory
  );

  return request;
}

function getCurrentUsage(usageHandler) {
  let principal = Cc["@mozilla.org/systemprincipal;1"].createInstance(
    Ci.nsIPrincipal
  );
  let request = SpecialPowers._getQuotaManager().getUsageForPrincipal(
    principal,
    usageHandler
  );

  return request;
}

function getPrincipal(url, attr = {}) {
  let uri = Cc["@mozilla.org/network/io-service;1"]
    .getService(Ci.nsIIOService)
    .newURI(url);
  let ssm = Cc["@mozilla.org/scriptsecuritymanager;1"].getService(
    Ci.nsIScriptSecurityManager
  );
  return ssm.createContentPrincipal(uri, attr);
}

var SpecialPowers = {
  getBoolPref(prefName) {
    return this._getPrefs().getBoolPref(prefName);
  },

  setBoolPref(prefName, value) {
    this._getPrefs().setBoolPref(prefName, value);
  },

  setIntPref(prefName, value) {
    this._getPrefs().setIntPref(prefName, value);
  },

  clearUserPref(prefName) {
    this._getPrefs().clearUserPref(prefName);
  },

  _getPrefs() {
    let prefService = Cc["@mozilla.org/preferences-service;1"].getService(
      Ci.nsIPrefService
    );
    return prefService.getBranch(null);
  },

  _getQuotaManager() {
    return Cc["@mozilla.org/dom/quota-manager-service;1"].getService(
      Ci.nsIQuotaManagerService
    );
  },
};

function installPackages(packageRelativePaths) {
  if (packageRelativePaths.length != 2) {
    throw new Error("Unsupported number of package relative paths");
  }

  for (const packageRelativePath of packageRelativePaths) {
    installPackage(packageRelativePath);
  }
}

// Take current storage structure on disk and compare it with the expected
// structure. The expected structure is defined in JSON and consists of a per
// test package definition and a shared package definition. The shared package
// definition should contain unknown stuff which needs to be properly handled
// in all situations.
function verifyStorage(packageDefinitionRelativePaths, key, sharedKey) {
  if (packageDefinitionRelativePaths.length != 2) {
    throw new Error("Unsupported number of package definition relative paths");
  }

  function verifyEntries(entries, name, indent = "") {
    log(`${indent}Verifying ${name} entries`);

    indent += "  ";

    for (const entry of entries) {
      const maybeName = entry.name;

      log(`${indent}Verifying entry ${maybeName}`);

      let hasName = false;
      let hasDir = false;
      let hasEntries = false;

      for (const property in entry) {
        switch (property) {
          case "note":
          case "todo":
            break;

          case "name":
            hasName = true;
            break;

          case "dir":
            hasDir = true;
            break;

          case "entries":
            hasEntries = true;
            break;

          default:
            throw new Error(`Unknown property ${property}`);
        }
      }

      if (!hasName) {
        throw new Error("An entry must have the name property");
      }

      if (!hasDir) {
        throw new Error("An entry must have the dir property");
      }

      if (hasEntries && !entry.dir) {
        throw new Error("An entry can't have entries if it's not a directory");
      }

      if (hasEntries) {
        verifyEntries(entry.entries, entry.name, indent);
      }
    }
  }

  function getCurrentEntries() {
    log("Getting current entries");

    function getEntryForFile(file) {
      let entry = {
        name: file.leafName,
        dir: file.isDirectory(),
      };

      if (file.isDirectory()) {
        const enumerator = file.directoryEntries;
        let nextFile;
        while ((nextFile = enumerator.nextFile)) {
          if (!entry.entries) {
            entry.entries = [];
          }
          entry.entries.push(getEntryForFile(nextFile));
        }
      }

      return entry;
    }

    let entries = [];

    let file = getRelativeFile("indexedDB");
    if (file.exists()) {
      entries.push(getEntryForFile(file));
    }

    file = getRelativeFile("storage");
    if (file.exists()) {
      entries.push(getEntryForFile(file));
    }

    file = getRelativeFile("storage.sqlite");
    if (file.exists()) {
      entries.push(getEntryForFile(file));
    }

    verifyEntries(entries, "current");

    return entries;
  }

  function getEntriesFromPackageDefinition(
    packageDefinitionRelativePath,
    lookupKey
  ) {
    log(`Getting ${lookupKey} entries from ${packageDefinitionRelativePath}`);

    const currentDir = Services.dirsvc.get("CurWorkD", Ci.nsIFile);
    const file = getRelativeFile(
      packageDefinitionRelativePath + ".json",
      currentDir
    );

    const fileInputStream = Cc[
      "@mozilla.org/network/file-input-stream;1"
    ].createInstance(Ci.nsIFileInputStream);
    fileInputStream.init(file, -1, -1, 0);

    const scriptableInputStream = Cc[
      "@mozilla.org/scriptableinputstream;1"
    ].createInstance(Ci.nsIScriptableInputStream);
    scriptableInputStream.init(fileInputStream);

    const data = scriptableInputStream.readBytes(
      scriptableInputStream.available()
    );

    const obj = JSON.parse(data);

    const result = obj.find(({ key: elementKey }) => elementKey == lookupKey);

    if (!result) {
      throw new Error("The file doesn't contain an element for given key");
    }

    if (!result.entries) {
      throw new Error("The element doesn't have the entries property");
    }

    verifyEntries(result.entries, lookupKey);

    return result.entries;
  }

  function addSharedEntries(expectedEntries, sharedEntries, name, indent = "") {
    log(`${indent}Checking common ${name} entries`);

    indent += "  ";

    for (const sharedEntry of sharedEntries) {
      const expectedEntry = expectedEntries.find(
        ({ name: elementName }) => elementName == sharedEntry.name
      );

      if (expectedEntry) {
        log(`${indent}Checking common entry ${sharedEntry.name}`);

        if (!expectedEntry.dir || !sharedEntry.dir) {
          throw new Error("A common entry must be a directory");
        }

        if (!expectedEntry.entries && !sharedEntry.entries) {
          throw new Error("A common entry must not be a leaf");
        }

        if (sharedEntry.entries) {
          if (!expectedEntry.entries) {
            expectedEntry.entries = [];
          }

          addSharedEntries(
            expectedEntry.entries,
            sharedEntry.entries,
            sharedEntry.name,
            indent
          );
        }
      } else {
        log(`${indent}Adding entry ${sharedEntry.name}`);
        expectedEntries.push(sharedEntry);
      }
    }
  }

  function compareEntries(currentEntries, expectedEntries, name, indent = "") {
    log(`${indent}Comparing ${name} entries`);

    indent += "  ";

    if (currentEntries.length != expectedEntries.length) {
      throw new Error("Entries must have the same length");
    }

    for (const currentEntry of currentEntries) {
      log(`${indent}Comparing entry ${currentEntry.name}`);

      const expectedEntry = expectedEntries.find(
        ({ name: elementName }) => elementName == currentEntry.name
      );

      if (!expectedEntry) {
        throw new Error("Cannot find a matching entry");
      }

      if (expectedEntry.dir != currentEntry.dir) {
        throw new Error("The dir property doesn't match");
      }

      if (
        (expectedEntry.entries && !currentEntry.entries) ||
        (!expectedEntry.entries && currentEntry.entries)
      ) {
        throw new Error("The entries property doesn't match");
      }

      if (expectedEntry.entries) {
        compareEntries(
          currentEntry.entries,
          expectedEntry.entries,
          currentEntry.name,
          indent
        );
      }
    }
  }

  const currentEntries = getCurrentEntries();

  log("Stringified current entries: " + JSON.stringify(currentEntries));

  const expectedEntries = getEntriesFromPackageDefinition(
    packageDefinitionRelativePaths[0],
    key
  );
  const sharedEntries = getEntriesFromPackageDefinition(
    packageDefinitionRelativePaths[1],
    sharedKey ? sharedKey : key
  );

  addSharedEntries(expectedEntries, sharedEntries, key);

  log("Stringified expected entries: " + JSON.stringify(expectedEntries));

  compareEntries(currentEntries, expectedEntries, key);
}

async function verifyInitializationStatus(
  expectStorageIsInitialized,
  expectTemporaryStorageIsInitialized
) {
  if (!expectStorageIsInitialized && expectTemporaryStorageIsInitialized) {
    throw new Error("Invalid expectation");
  }

  let request = storageInitialized();
  await requestFinished(request);

  const storageIsInitialized = request.result;

  request = temporaryStorageInitialized();
  await requestFinished(request);

  const temporaryStorageIsInitialized = request.result;

  ok(
    !(!storageIsInitialized && temporaryStorageIsInitialized),
    "Initialization status is consistent"
  );

  if (expectStorageIsInitialized) {
    ok(storageIsInitialized, "Storage is initialized");
  } else {
    ok(!storageIsInitialized, "Storage is not initialized");
  }

  if (expectTemporaryStorageIsInitialized) {
    ok(temporaryStorageIsInitialized, "Temporary storage is initialized");
  } else {
    ok(!temporaryStorageIsInitialized, "Temporary storage is not initialized");
  }
}
