/* eslint-env mozilla/chrome-script */

// eslint-disable-next-line mozilla/reject-importGlobalProperties
Cu.importGlobalProperties(["File"]);

addMessageListener("file.open", function (e) {
  var tmpFile = Cc["@mozilla.org/file/directory_service;1"]
    .getService(Ci.nsIDirectoryService)
    .QueryInterface(Ci.nsIProperties)
    .get("TmpD", Ci.nsIFile);
  tmpFile.append("file.txt");
  tmpFile.createUnique(Ci.nsIFile.NORMAL_FILE_TYPE, 0o600);

  File.createFromNsIFile(tmpFile).then(function (file) {
    sendAsyncMessage("file.opened", { data: file });
  });
});

addMessageListener("nonEmptyFile.open", function (e) {
  var tmpFile = Cc["@mozilla.org/file/directory_service;1"]
    .getService(Ci.nsIDirectoryService)
    .QueryInterface(Ci.nsIProperties)
    .get("TmpD", Ci.nsIFile);
  tmpFile.append("file.txt");
  tmpFile.createUnique(Ci.nsIFile.NORMAL_FILE_TYPE, 0o600);

  var outStream = Cc[
    "@mozilla.org/network/file-output-stream;1"
  ].createInstance(Ci.nsIFileOutputStream);
  outStream.init(
    tmpFile,
    0x02 | 0x08 | 0x20, // write, create, truncate
    0o666,
    0
  );
  var fileData = "Hello world!";
  outStream.write(fileData, fileData.length);
  outStream.close();

  File.createFromNsIFile(tmpFile).then(function (file) {
    sendAsyncMessage("nonEmptyFile.opened", { data: file });
  });
});
