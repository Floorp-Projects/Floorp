/**
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 *
 * All images in schema_15_profile.zip are from https://github.com/mdn/sw-test/
 * and are CC licensed by https://www.flickr.com/photos/legofenris/.
 */

// testSteps is expected to be defined by the file including this file.
/* global testSteps */

const { Services } = ChromeUtils.import("resource://gre/modules/Services.jsm");

// services required be initialized in order to run CacheStorage
var ss = Cc["@mozilla.org/storage/service;1"].createInstance(
  Ci.mozIStorageService
);
var sts = Cc["@mozilla.org/network/stream-transport-service;1"].getService(
  Ci.nsIStreamTransportService
);
var hash = Cc["@mozilla.org/security/hash;1"].createInstance(Ci.nsICryptoHash);

function run_test() {
  runTest();
}

function runTest() {
  do_get_profile();

  // Expose Cache and Fetch symbols on the global
  Cu.importGlobalProperties(["caches", "fetch"]);

  Assert.ok(
    typeof testSteps === "function",
    "There should be a testSteps function"
  );
  Assert.ok(
    testSteps.constructor.name === "AsyncFunction",
    "testSteps should be an async function"
  );

  add_task(testSteps);

  // Since we defined run_test, we must invoke run_next_test() to start the
  // async test.
  run_next_test();
}

// Extract a zip file into the profile
function create_test_profile(zipFileName) {
  var directoryService = Services.dirsvc;

  var profileDir = directoryService.get("ProfD", Ci.nsIFile);
  var currentDir = directoryService.get("CurWorkD", Ci.nsIFile);

  var packageFile = currentDir.clone();
  packageFile.append(zipFileName);

  var zipReader = Cc["@mozilla.org/libjar/zip-reader;1"].createInstance(
    Ci.nsIZipReader
  );
  zipReader.open(packageFile);

  var entryNames = Array.from(zipReader.findEntries(null));
  entryNames.sort();

  for (var entryName of entryNames) {
    var zipentry = zipReader.getEntry(entryName);

    var file = profileDir.clone();
    entryName.split("/").forEach(function(part) {
      file.append(part);
    });

    if (zipentry.isDirectory) {
      file.create(Ci.nsIFile.DIRECTORY_TYPE, parseInt("0755", 8));
    } else {
      var istream = zipReader.getInputStream(entryName);

      var ostream = Cc[
        "@mozilla.org/network/file-output-stream;1"
      ].createInstance(Ci.nsIFileOutputStream);
      ostream.init(file, -1, parseInt("0644", 8), 0);

      var bostream = Cc[
        "@mozilla.org/network/buffered-output-stream;1"
      ].createInstance(Ci.nsIBufferedOutputStream);
      bostream.init(ostream, 32 * 1024);

      bostream.writeFrom(istream, istream.available());

      istream.close();
      bostream.close();
    }
  }

  zipReader.close();
}

function getCacheDir() {
  let dirService = Services.dirsvc;

  let profileDir = dirService.get("ProfD", Ci.nsIFile);
  let cacheDir = profileDir.clone();
  cacheDir.append("storage");
  cacheDir.append("default");
  cacheDir.append("chrome");
  cacheDir.append("cache");

  return cacheDir;
}
