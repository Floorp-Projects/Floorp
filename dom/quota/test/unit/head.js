/**
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

const NS_OK = Cr.NS_OK;
const NS_ERROR_FAILURE = Cr.NS_ERROR_FAILURE;
const NS_ERROR_UNEXPECTED = Cr.NS_ERROR_UNEXPECTED;
const NS_ERROR_STORAGE_BUSY = Cr.NS_ERROR_STORAGE_BUSY;

function is(a, b, msg)
{
  Assert.equal(a, b, msg);
}

function ok(cond, msg)
{
  Assert.ok(!!cond, msg);
}

function run_test()
{
  runTest();
};

if (!this.runTest) {
  this.runTest = function()
  {
    do_get_profile();

    enableTesting();

    Cu.importGlobalProperties(["indexedDB", "File", "Blob", "FileReader"]);

    do_test_pending();
    testGenerator.next();
  }
}

function finishTest()
{
  resetTesting();

  executeSoon(function() {
    do_test_finished();
  })
}

function grabArgAndContinueHandler(arg)
{
  testGenerator.next(arg);
}

function continueToNextStep()
{
  executeSoon(function() {
    testGenerator.next();
  });
}

function continueToNextStepSync()
{
  testGenerator.next();
}

function enableTesting()
{
  SpecialPowers.setBoolPref("dom.quotaManager.testing", true);
  SpecialPowers.setBoolPref("dom.simpleDB.enabled", true);
}

function resetTesting()
{
  SpecialPowers.clearUserPref("dom.quotaManager.testing");
  SpecialPowers.clearUserPref("dom.simpleDB.enabled");
}

function init(callback)
{
  let request = SpecialPowers._getQuotaManager().init();
  request.callback = callback;

  return request;
}

function initOrigin(principal, persistence, callback)
{
  let request =
    SpecialPowers._getQuotaManager().initStoragesForPrincipal(principal,
                                                              persistence);
  request.callback = callback;

  return request;
}

function initChromeOrigin(persistence, callback)
{
  let principal = Cc["@mozilla.org/systemprincipal;1"]
                    .createInstance(Ci.nsIPrincipal);
  let request =
    SpecialPowers._getQuotaManager().initStoragesForPrincipal(principal,
                                                              persistence);
  request.callback = callback;

  return request;
}

function clear(callback)
{
  let request = SpecialPowers._getQuotaManager().clear();
  request.callback = callback;

  return request;
}

function clearOrigin(principal, persistence, callback)
{
  let request =
    SpecialPowers._getQuotaManager().clearStoragesForPrincipal(principal,
                                                               persistence);
  request.callback = callback;

  return request;
}

function clearChromeOrigin(callback)
{
  let principal = Cc["@mozilla.org/systemprincipal;1"]
                    .createInstance(Ci.nsIPrincipal);
  let request =
    SpecialPowers._getQuotaManager().clearStoragesForPrincipal(principal,
                                                              "persistent");
  request.callback = callback;

  return request;
}

function reset(callback)
{
  let request = SpecialPowers._getQuotaManager().reset();
  request.callback = callback;

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

function installPackage(packageName)
{
  let directoryService = Cc["@mozilla.org/file/directory_service;1"]
                         .getService(Ci.nsIProperties);

  let currentDir = directoryService.get("CurWorkD", Ci.nsIFile);

  let packageFile = currentDir.clone();
  packageFile.append(packageName + ".zip");

  let zipReader = Cc["@mozilla.org/libjar/zip-reader;1"]
                  .createInstance(Ci.nsIZipReader);
  zipReader.open(packageFile);

  let entryNames = Array.from(zipReader.findEntries(null));
  entryNames.sort();

  for (let entryName of entryNames) {
    let zipentry = zipReader.getEntry(entryName);

    let file = getRelativeFile(entryName);

    if (zipentry.isDirectory) {
      file.create(Ci.nsIFile.DIRECTORY_TYPE, parseInt("0755", 8));
    } else {
      let istream = zipReader.getInputStream(entryName);

      var ostream = Cc["@mozilla.org/network/file-output-stream;1"]
                    .createInstance(Ci.nsIFileOutputStream);
      ostream.init(file, -1, parseInt("0644", 8), 0);

      let bostream = Cc['@mozilla.org/network/buffered-output-stream;1']
                     .createInstance(Ci.nsIBufferedOutputStream);
      bostream.init(ostream, 32768);

      bostream.writeFrom(istream, istream.available());

      istream.close();
      bostream.close();
    }
  }

  zipReader.close();
}

function getProfileDir()
{
  let directoryService =
    Cc["@mozilla.org/file/directory_service;1"].getService(Ci.nsIProperties);

  return directoryService.get("ProfD", Ci.nsIFile);
}

// Given a "/"-delimited path relative to the profile directory,
// return an nsIFile representing the path.  This does not test
// for the existence of the file or parent directories.
// It is safe even on Windows where the directory separator is not "/",
// but make sure you're not passing in a "\"-delimited path.
function getRelativeFile(relativePath)
{
  let profileDir = getProfileDir();

  let file = profileDir.clone();
  relativePath.split('/').forEach(function(component) {
    file.append(component);
  });

  return file;
}

function getPersistedFromMetadata(readBuffer)
{
  const persistedPosition = 8; // Persisted state is stored in the 9th byte
  let view =
    readBuffer instanceof Uint8Array ? readBuffer : new Uint8Array(readBuffer);

  return !!view[persistedPosition];
}

function grabResultAndContinueHandler(request)
{
  testGenerator.next(request.result);
}

function grabUsageAndContinueHandler(request)
{
  testGenerator.next(request.result.usage);
}

function getUsage(usageHandler, getAll)
{
  let request = SpecialPowers._getQuotaManager().getUsage(usageHandler, getAll);

  return request;
}

function getCurrentUsage(usageHandler)
{
  let principal = Cc["@mozilla.org/systemprincipal;1"]
                    .createInstance(Ci.nsIPrincipal);
  let request =
    SpecialPowers._getQuotaManager().getUsageForPrincipal(principal,
                                                          usageHandler);

  return request;
}

function getPrincipal(url)
{
  let uri = Cc["@mozilla.org/network/io-service;1"]
              .getService(Ci.nsIIOService)
              .newURI(url);
  let ssm = Cc["@mozilla.org/scriptsecuritymanager;1"]
              .getService(Ci.nsIScriptSecurityManager);
  return ssm.createCodebasePrincipal(uri, {});
}

function getCurrentPrincipal()
{
  return Cc["@mozilla.org/systemprincipal;1"].createInstance(Ci.nsIPrincipal);
}

function getSimpleDatabase(principal)
{
  let connection = Cc["@mozilla.org/dom/sdb-connection;1"]
    .createInstance(Ci.nsISDBConnection);

  if (!principal) {
    principal = getCurrentPrincipal();
  }

  connection.init(principal);

  return connection;
}

function* requestFinished(request) {
  request.callback = continueToNextStepSync;
  yield undefined;
  if (request.resultCode == NS_OK) {
    return request.result;
  }
  throw request.resultCode;
}

var SpecialPowers = {
  getBoolPref: function(prefName) {
    return this._getPrefs().getBoolPref(prefName);
  },

  setBoolPref: function(prefName, value) {
    this._getPrefs().setBoolPref(prefName, value);
  },

  clearUserPref: function(prefName) {
    this._getPrefs().clearUserPref(prefName);
  },

  _getPrefs: function() {
    let prefService =
      Cc["@mozilla.org/preferences-service;1"].getService(Ci.nsIPrefService);
    return prefService.getBranch(null);
  },

  _getQuotaManager: function() {
    return Cc["@mozilla.org/dom/quota-manager-service;1"]
             .getService(Ci.nsIQuotaManagerService);
  },
};

function loadSubscript(path)
{
  let file = do_get_file(path, false);
  let uri = Cc["@mozilla.org/network/io-service;1"]
    .getService(Ci.nsIIOService).newFileURI(file);
  let scriptLoader = Cc["@mozilla.org/moz/jssubscript-loader;1"]
    .getService(Ci.mozIJSSubScriptLoader);
  scriptLoader.loadSubScript(uri.spec);
}

loadSubscript("../head-shared.js");
