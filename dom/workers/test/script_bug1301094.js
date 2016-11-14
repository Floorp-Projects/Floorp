var { classes: Cc, interfaces: Ci, utils: Cu } = Components;
Cu.importGlobalProperties(["File", "Directory"]);

addMessageListener("file.open", function (e) {
  var tmpFile = Cc["@mozilla.org/file/directory_service;1"]
                  .getService(Ci.nsIDirectoryService)
                  .QueryInterface(Ci.nsIProperties)
                  .get('TmpD', Ci.nsIFile)
  tmpFile.append('file.txt');
  tmpFile.createUnique(Components.interfaces.nsIFile.FILE_TYPE, 0o600);

  sendAsyncMessage("file.opened", {
    data: File.createFromNsIFile(tmpFile)
  });
});
