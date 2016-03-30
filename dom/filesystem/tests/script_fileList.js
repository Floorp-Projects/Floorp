var { classes: Cc, interfaces: Ci, utils: Cu } = Components;
Cu.importGlobalProperties(["File"]);

addMessageListener("dir.open", function (e) {
  var testFile = Cc["@mozilla.org/file/directory_service;1"]
                   .getService(Ci.nsIDirectoryService)
                   .QueryInterface(Ci.nsIProperties)
                   .get(e.path == 'root' ? 'ProfD' : e.path, Ci.nsIFile);

  // Let's go back to the root of the FileSystem
  if (e.path == 'root') {
    while (true) {
      var parent = testFile.parent;
      if (!parent) {
        break;
      }

      testFile = parent;
    }
  }

  sendAsyncMessage("dir.opened", {
    dir: testFile.path
  });
});
