/* eslint-env mozilla/chrome-script */

// eslint-disable-next-line mozilla/reject-importGlobalProperties
Cu.importGlobalProperties(["File"]);

addMessageListener("file.open", function () {
  var testFile = Cc["@mozilla.org/file/directory_service;1"]
    .getService(Ci.nsIDirectoryService)
    .QueryInterface(Ci.nsIProperties)
    .get("ProfD", Ci.nsIFile);
  testFile.append("prefs.js");

  File.createFromNsIFile(testFile).then(function (file) {
    sendAsyncMessage("file.opened", { file });
  });
});

addMessageListener("dir.open", function () {
  var testFile = Cc["@mozilla.org/file/directory_service;1"]
    .getService(Ci.nsIDirectoryService)
    .QueryInterface(Ci.nsIProperties)
    .get("ProfD", Ci.nsIFile);

  sendAsyncMessage("dir.opened", {
    dir: testFile.path,
  });
});
