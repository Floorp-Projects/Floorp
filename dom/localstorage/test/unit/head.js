/**
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

// Tests are expected to define testSteps.
/* globals testSteps */

const NS_ERROR_DOM_QUOTA_EXCEEDED_ERR = 22;

var { Services } = ChromeUtils.import("resource://gre/modules/Services.jsm");

function is(a, b, msg) {
  Assert.equal(a, b, msg);
}

function ok(cond, msg) {
  Assert.ok(!!cond, msg);
}

function run_test() {
  runTest();
}

if (!this.runTest) {
  this.runTest = function() {
    do_get_profile();

    enableTesting();

    Cu.importGlobalProperties(["crypto"]);

    Assert.ok(
      typeof testSteps === "function",
      "There should be a testSteps function"
    );
    Assert.ok(
      testSteps.constructor.name === "AsyncFunction",
      "testSteps should be an async function"
    );

    registerCleanupFunction(resetTesting);

    add_task(testSteps);

    // Since we defined run_test, we must invoke run_next_test() to start the
    // async test.
    run_next_test();
  };
}

function returnToEventLoop() {
  return new Promise(function(resolve) {
    executeSoon(resolve);
  });
}

function enableTesting() {
  Services.prefs.setBoolPref("dom.simpleDB.enabled", true);
  Services.prefs.setBoolPref("dom.storage.testing", true);

  // xpcshell globals don't have associated clients in the Clients API sense, so
  // we need to disable client validation so that the unit tests are allowed to
  // use LocalStorage.
  Services.prefs.setBoolPref("dom.storage.client_validation", false);

  Services.prefs.setBoolPref("dom.quotaManager.testing", true);
}

function resetTesting() {
  Services.prefs.clearUserPref("dom.quotaManager.testing");
  Services.prefs.clearUserPref("dom.storage.client_validation");
  Services.prefs.clearUserPref("dom.storage.testing");
  Services.prefs.clearUserPref("dom.simpleDB.enabled");
}

function setGlobalLimit(globalLimit) {
  Services.prefs.setIntPref(
    "dom.quotaManager.temporaryStorage.fixedLimit",
    globalLimit
  );
}

function resetGlobalLimit() {
  Services.prefs.clearUserPref("dom.quotaManager.temporaryStorage.fixedLimit");
}

function setOriginLimit(originLimit) {
  Services.prefs.setIntPref("dom.storage.default_quota", originLimit);
}

function resetOriginLimit() {
  Services.prefs.clearUserPref("dom.storage.default_quota");
}

function setTimeout(callback, timeout) {
  let timer = Cc["@mozilla.org/timer;1"].createInstance(Ci.nsITimer);

  timer.initWithCallback(
    {
      notify() {
        callback();
      },
    },
    timeout,
    Ci.nsITimer.TYPE_ONE_SHOT
  );

  return timer;
}

function initStorage() {
  return Services.qms.init();
}

function initTemporaryStorage() {
  return Services.qms.initTemporaryStorage();
}

function initPersistentOrigin(principal) {
  return Services.qms.initializePersistentOrigin(principal);
}

function initTemporaryOrigin(persistence, principal) {
  return Services.qms.initializeTemporaryOrigin(persistence, principal);
}

function getOriginUsage(principal, fromMemory = false) {
  let request = Services.qms.getUsageForPrincipal(
    principal,
    function() {},
    fromMemory
  );

  return request;
}

function clear() {
  let request = Services.qms.clear();

  return request;
}

function clearOriginsByPattern(pattern) {
  let request = Services.qms.clearStoragesForOriginAttributesPattern(pattern);

  return request;
}

function clearOriginsByPrefix(principal, persistence) {
  let request = Services.qms.clearStoragesForPrincipal(
    principal,
    persistence,
    null,
    true
  );

  return request;
}

function clearOrigin(principal, persistence) {
  let request = Services.qms.clearStoragesForPrincipal(principal, persistence);

  return request;
}

function reset() {
  let request = Services.qms.reset();

  return request;
}

function resetOrigin(principal) {
  let request = Services.qms.resetStoragesForPrincipal(
    principal,
    "default",
    "ls"
  );

  return request;
}

function installPackage(packageName) {
  let currentDir = Services.dirsvc.get("CurWorkD", Ci.nsIFile);

  let packageFile = currentDir.clone();
  packageFile.append(packageName + ".zip");

  let zipReader = Cc["@mozilla.org/libjar/zip-reader;1"].createInstance(
    Ci.nsIZipReader
  );
  zipReader.open(packageFile);

  let entryNames = [];
  let entries = zipReader.findEntries(null);
  while (entries.hasMore()) {
    let entry = entries.getNext();
    entryNames.push(entry);
  }
  entryNames.sort();

  for (let entryName of entryNames) {
    let zipentry = zipReader.getEntry(entryName);

    let file = getRelativeFile(entryName);

    if (zipentry.isDirectory) {
      file.create(Ci.nsIFile.DIRECTORY_TYPE, parseInt("0755", 8));
    } else {
      let istream = zipReader.getInputStream(entryName);

      var ostream = Cc[
        "@mozilla.org/network/file-output-stream;1"
      ].createInstance(Ci.nsIFileOutputStream);
      ostream.init(file, -1, parseInt("0644", 8), 0);

      let bostream = Cc[
        "@mozilla.org/network/buffered-output-stream;1"
      ].createInstance(Ci.nsIBufferedOutputStream);
      bostream.init(ostream, 32768);

      bostream.writeFrom(istream, istream.available());

      istream.close();
      bostream.close();
    }
  }

  zipReader.close();
}

function getProfileDir() {
  return Services.dirsvc.get("ProfD", Ci.nsIFile);
}

// Given a "/"-delimited path relative to the profile directory,
// return an nsIFile representing the path.  This does not test
// for the existence of the file or parent directories.
// It is safe even on Windows where the directory separator is not "/",
// but make sure you're not passing in a "\"-delimited path.
function getRelativeFile(relativePath) {
  let profileDir = getProfileDir();

  let file = profileDir.clone();
  relativePath.split("/").forEach(function(component) {
    file.append(component);
  });

  return file;
}

function repeatChar(count, ch) {
  if (count == 0) {
    return "";
  }

  let result = ch;
  let count2 = count / 2;

  // Double the input until it is long enough.
  while (result.length <= count2) {
    result += result;
  }

  // Use substring to hit the precise length target without using extra memory.
  return result + result.substring(0, count - result.length);
}

function getPrincipal(url, attrs) {
  let uri = Services.io.newURI(url);
  if (!attrs) {
    attrs = {};
  }
  return Services.scriptSecurityManager.createContentPrincipal(uri, attrs);
}

function getCurrentPrincipal() {
  return Cc["@mozilla.org/systemprincipal;1"].createInstance(Ci.nsIPrincipal);
}

function getDefaultPrincipal() {
  return getPrincipal("http://example.com");
}

function getSimpleDatabase(principal, persistence) {
  let connection = Cc["@mozilla.org/dom/sdb-connection;1"].createInstance(
    Ci.nsISDBConnection
  );

  if (!principal) {
    principal = getDefaultPrincipal();
  }

  connection.init(principal, persistence);

  return connection;
}

function getLocalStorage(principal) {
  if (!principal) {
    principal = getDefaultPrincipal();
  }

  return Services.domStorageManager.createStorage(
    null,
    principal,
    principal,
    ""
  );
}

class RequestError extends Error {
  constructor(resultCode, resultName) {
    super(`Request failed (code: ${resultCode}, name: ${resultName})`);
    this.name = "RequestError";
    this.resultCode = resultCode;
    this.resultName = resultName;
  }
}

async function requestFinished(request) {
  await new Promise(function(resolve) {
    request.callback = function() {
      resolve();
    };
  });

  if (request.resultCode !== Cr.NS_OK) {
    throw new RequestError(request.resultCode, request.resultName);
  }

  return request.result;
}

function loadSubscript(path) {
  let file = do_get_file(path, false);
  let uri = Services.io.newFileURI(file);
  Services.scriptloader.loadSubScript(uri.spec);
}

async function readUsageFromUsageFile(usageFile) {
  let file = await File.createFromNsIFile(usageFile);

  let buffer = await new Promise(resolve => {
    let reader = new FileReader();
    reader.onloadend = () => resolve(reader.result);
    reader.readAsArrayBuffer(file);
  });

  // Manually getting the lower 32-bits because of the lack of support for
  // 64-bit values currently from DataView/JS (other than the BigInt support
  // that's currently behind a flag).
  let view = new DataView(buffer, 8, 4);
  return view.getUint32();
}
