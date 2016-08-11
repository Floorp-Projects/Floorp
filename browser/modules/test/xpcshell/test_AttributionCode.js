/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */
"use strict";

const {classes: Cc, interfaces: Ci, utils: Cu} = Components;

Cu.import("resource://gre/modules/AppConstants.jsm");
Cu.import("resource:///modules/AttributionCode.jsm");
Cu.import("resource://gre/modules/Services.jsm");

function getAttributionFile() {
  let directoryService = Cc["@mozilla.org/file/directory_service;1"].
                           getService(Ci.nsIProperties);
  let file = directoryService.get("LocalAppData", Ci.nsIFile);
  file.append(Services.appinfo.vendor || "mozilla");
  file.append(AppConstants.MOZ_APP_NAME);
  file.append("postSigningData");
  return file;
}

function writeAttributionFile(data) {
  let stream = Cc["@mozilla.org/network/file-output-stream;1"].
                 createInstance(Ci.nsIFileOutputStream);
  stream.init(getAttributionFile(), -1, -1, 0);
  stream.write(data, data.length);
  stream.close();
}

let attrCodeList = [
  {code: "source%3Dgoogle.com%26medium%3Dorganic%26campaign%3D(not%20set)%26content%3D(not%20set)",
   valid: true},
  {code: "source%3Dgoogle.com%26medium%3Dorganic%26campaign%3D%26content%3D",
   valid: true},
  {code: "",
   valid: false},
  {code: "source=google.com&medium=organic&campaign=(not set)&content=(not set)",
   valid: false},
  {code: "source%3Dgoogle.com%26medium%3Dorganic%26campaign%3D(not%20set)",
   valid: false},
  {code: "source%3Dgoogle.com%26medium%3Dorganic",
   valid: false},
  {code: "source%3Dgoogle.com",
   valid: false},
  {code: "source%reallyreallyreallyreallyreallyreallyreallyreallyreallylongdomain.com%26medium%3Dorganic%26campaign%3D(not%20set)%26content%3Dalmostexactlyenoughcontenttomakethisstringlongerthanthe200characterlimit",
   valid: false},
];

/**
 * Test validation of attribution codes,
 * to make sure we reject bad ones and accept good ones.
 */
add_task(function* testAttrCodes() {
  for (let entry of attrCodeList) {
    AttributionCode._clearCache();
    writeAttributionFile(entry.code);
    let result = yield AttributionCode.getCodeAsync();
    if (entry.valid) {
      Assert.equal(result, entry.code, "Code should be valid");
    } else {
      Assert.equal(result, "", "Code should not be valid");
    }
  }
  AttributionCode._clearCache();
});

/**
 * Test the cache by deleting the attribution data file
 * and making sure we still get the expected code.
 */
add_task(function* testDeletedFile() {
  // Set up the test by clearing the cache and writing a valid file.
  const code =
    "source%3Dgoogle.com%26medium%3Dorganic%26campaign%3D(not%20set)%26content%3D(not%20set)";
  writeAttributionFile(code);
  let result = yield AttributionCode.getCodeAsync();
  Assert.equal(result, code,
    "The code should be readable directly from the file");

  // Delete the file and make sure we can still read the value back from cache.
  yield AttributionCode.deleteFileAsync();
  result = yield AttributionCode.getCodeAsync();
  Assert.equal(result, code,
    "The code should be readable from the cache");

  // Clear the cache and check we can't read anything.
  AttributionCode._clearCache();
  result = yield AttributionCode.getCodeAsync();
  Assert.equal(result, "",
    "Shouldn't be able to get a code after file is deleted and cache is cleared");
});
