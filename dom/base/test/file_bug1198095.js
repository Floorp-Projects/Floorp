var { classes: Cc, interfaces: Ci, utils: Cu } = Components;
Cu.importGlobalProperties(["File"]);

function createFileWithData(message) {
  var dirSvc = Cc["@mozilla.org/file/directory_service;1"].getService(Ci.nsIProperties);
  var testFile = dirSvc.get("ProfD", Ci.nsIFile);
  testFile.append("fileAPItestfileBug1198095");

  var outStream = Cc["@mozilla.org/network/file-output-stream;1"].createInstance(Ci.nsIFileOutputStream);
  outStream.init(testFile, 0x02 | 0x08 | 0x20, // write, create, truncate
                 0666, 0);

  outStream.write(message, message.length);
  outStream.close();

  var domFile = new File(testFile);
  return domFile;
}

addMessageListener("file.open", function (message) {
  sendAsyncMessage("file.opened", createFileWithData(message));
});

addMessageListener("file.modify", function (message) {
  sendAsyncMessage("file.modified", createFileWithData(message));
});
