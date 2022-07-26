/* eslint-env mozilla/chrome-script */

Cu.importGlobalProperties(["File"]);

function createFileWithData(message) {
  var dirSvc = Cc["@mozilla.org/file/directory_service;1"].getService(
    Ci.nsIProperties
  );
  var testFile = dirSvc.get("ProfD", Ci.nsIFile);
  testFile.append("fileAPItestfileBug1198095");

  var outStream = Cc[
    "@mozilla.org/network/file-output-stream;1"
  ].createInstance(Ci.nsIFileOutputStream);
  outStream.init(
    testFile,
    0x02 | 0x08 | 0x20, // write, create, truncate
    0o666,
    0
  );

  outStream.write(message, message.length);
  outStream.close();

  return File.createFromNsIFile(testFile);
}

addMessageListener("file.open", function(message) {
  createFileWithData(message).then(function(file) {
    sendAsyncMessage("file.opened", file);
  });
});

addMessageListener("file.modify", function(message) {
  createFileWithData(message).then(function(file) {
    sendAsyncMessage("file.modified", file);
  });
});
