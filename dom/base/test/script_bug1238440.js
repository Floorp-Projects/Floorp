/* eslint-env mozilla/chrome-script */

Cu.importGlobalProperties(["File"]);

var tmpFile;

function writeFile(text, answer) {
  var stream = Cc["@mozilla.org/network/file-output-stream;1"].createInstance(
    Ci.nsIFileOutputStream
  );
  stream.init(tmpFile, 0x02 | 0x08 | 0x10, 0o600, 0);
  stream.write(text, text.length);
  stream.close();

  File.createFromNsIFile(tmpFile).then(function(file) {
    sendAsyncMessage(answer, { file });
  });
}

addMessageListener("file.open", function(e) {
  tmpFile = Cc["@mozilla.org/file/directory_service;1"]
    .getService(Ci.nsIDirectoryService)
    .QueryInterface(Ci.nsIProperties)
    .get("TmpD", Ci.nsIFile);
  tmpFile.append("foo.txt");
  tmpFile.createUnique(Ci.nsIFile.NORMAL_FILE_TYPE, 0o600);

  writeFile("hello world", "file.opened");
});

addMessageListener("file.change", function(e) {
  writeFile("hello world---------------", "file.changed");
});
