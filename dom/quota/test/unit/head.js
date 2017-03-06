/**
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

var { 'classes': Cc, 'interfaces': Ci, 'utils': Cu } = Components;

function is(a, b, msg)
{
  do_check_eq(a, b, Components.stack.caller);
}

function ok(cond, msg)
{
  do_check_true(!!cond, Components.stack.caller);
}

function info(name, message)
{
  do_print(name);
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

    Cu.importGlobalProperties(["indexedDB"]);

    do_test_pending();
    testGenerator.next();
  }
}

function finishTest()
{
  resetTesting();

  do_execute_soon(function() {
    do_test_finished();
  })
}

function continueToNextStep()
{
  do_execute_soon(function() {
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
}

function resetTesting()
{
  SpecialPowers.clearUserPref("dom.quotaManager.testing");
}

function clear(callback)
{
  let request = SpecialPowers._getQuotaManager().clear();
  request.callback = callback;

  return request;
}

function reset(callback)
{
  let request = SpecialPowers._getQuotaManager().reset();
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

function grabUsageAndContinueHandler(request)
{
  testGenerator.next(request.usage);
}

function getUsage(usageHandler)
{
  let principal = Cc["@mozilla.org/systemprincipal;1"]
                    .createInstance(Ci.nsIPrincipal);
  let request =
    SpecialPowers._getQuotaManager().getUsageForPrincipal(principal,
                                                          usageHandler);

  return request;
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
