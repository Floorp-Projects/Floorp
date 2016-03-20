var { classes: Cc, interfaces: Ci, utils: Cu } = Components;
Cu.importGlobalProperties(["File"]);

addMessageListener("dir.open", function () {
  var testFile = Cc["@mozilla.org/file/directory_service;1"]
                   .getService(Ci.nsIDirectoryService)
                   .QueryInterface(Ci.nsIProperties)
                   .get("ProfD", Ci.nsIFile);

  sendAsyncMessage("dir.opened", {
    dir: testFile.path
  });
});
