var { classes: Cc, interfaces: Ci, utils: Cu } = Components;
Cu.importGlobalProperties(["File"]);

var fileNum = 1;

function createFileWithData(fileData) {
  var willDelete = fileData === null;
  var dirSvc = Cc["@mozilla.org/file/directory_service;1"].getService(Ci.nsIProperties);
  var testFile = dirSvc.get("ProfD", Ci.nsIFile);
  testFile.append("fileAPItestfile" + fileNum);
  fileNum++;
  var outStream = Cc["@mozilla.org/network/file-output-stream;1"].createInstance(Ci.nsIFileOutputStream);
  outStream.init(testFile, 0x02 | 0x08 | 0x20, // write, create, truncate
                 0o666, 0);
  if (willDelete) {
    fileData = "some irrelevant test data\n";
  }
  outStream.write(fileData, fileData.length);
  outStream.close();
  var domFile = File.createFromNsIFile(testFile);
  if (willDelete) {
    testFile.remove(/* recursive: */ false);
  }
  return domFile;
}

addMessageListener("files.open", function (message) {
  sendAsyncMessage("files.opened", message.map(createFileWithData));
});
