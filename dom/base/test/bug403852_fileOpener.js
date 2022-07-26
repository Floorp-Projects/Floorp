/* eslint-env mozilla/chrome-script */

Cu.importGlobalProperties(["File"]);

var testFile = Cc["@mozilla.org/file/directory_service;1"]
  .getService(Ci.nsIDirectoryService)
  .QueryInterface(Ci.nsIProperties)
  .get("ProfD", Ci.nsIFile);
testFile.append("prefs.js");

addMessageListener("file.open", function() {
  File.createFromNsIFile(testFile).then(function(file) {
    File.createFromNsIFile(testFile, { lastModified: 123 }).then(function(
      fileWithDate
    ) {
      sendAsyncMessage("file.opened", {
        file,
        mtime: testFile.lastModifiedTime,
        fileWithDate,
        fileDate: 123,
      });
    });
  });
});
