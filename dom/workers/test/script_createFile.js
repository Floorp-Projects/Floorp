Cu.importGlobalProperties(["File", "Directory"]);

addMessageListener("file.open", function (e) {
  var tmpFile = Cc["@mozilla.org/file/directory_service;1"]
                  .getService(Ci.nsIDirectoryService)
                  .QueryInterface(Ci.nsIProperties)
                  .get('TmpD', Ci.nsIFile)
  tmpFile.append('file.txt');
  tmpFile.createUnique(Ci.nsIFile.FILE_TYPE, 0o600);

  File.createFromNsIFile(tmpFile).then(function(file) {
    sendAsyncMessage("file.opened", { data: file });
  });
});
