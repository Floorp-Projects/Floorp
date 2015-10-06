var { classes: Cc, interfaces: Ci, utils: Cu } = Components;
Cu.importGlobalProperties(["File"]);

var testFile = Cc["@mozilla.org/file/directory_service;1"]
                 .getService(Ci.nsIDirectoryService)
                 .QueryInterface(Ci.nsIProperties)
                 .get("ProfD", Ci.nsIFile);
testFile.append("prefs.js");

addMessageListener("file.open", function () {
  sendAsyncMessage("file.opened", {
    file: new File(testFile),
    mtime: testFile.lastModifiedTime,
    fileWithDate: new File(testFile, { lastModified: 123 }),
    fileDate: 123,
  });
});
