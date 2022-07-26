/* eslint-env mozilla/chrome-script */
// eslint-disable-next-line mozilla/reject-importGlobalProperties
Cu.importGlobalProperties(["File", "Directory"]);
var tmpFile, tmpDir;

addMessageListener("entries.open", function(e) {
  tmpFile = Services.dirsvc
    .QueryInterface(Ci.nsIProperties)
    .get("TmpD", Ci.nsIFile);
  tmpFile.append("file.txt");
  tmpFile.createUnique(Ci.nsIFile.NORMAL_FILE_TYPE, 0o600);

  tmpDir = Services.dirsvc
    .QueryInterface(Ci.nsIProperties)
    .get("TmpD", Ci.nsIFile);

  tmpDir.append("dir-test");
  tmpDir.createUnique(Ci.nsIFile.DIRECTORY_TYPE, 0o700);

  var file1 = tmpDir.clone();
  file1.append("foo.txt");
  file1.create(Ci.nsIFile.NORMAL_FILE_TYPE, 0o600);

  var dir1 = tmpDir.clone();
  dir1.append("subdir");
  dir1.create(Ci.nsIFile.DIRECTORY_TYPE, 0o700);

  var file2 = dir1.clone();
  file2.append("bar..txt"); // Note the double ..
  file2.create(Ci.nsIFile.NORMAL_FILE_TYPE, 0o600);

  var dir2 = dir1.clone();
  dir2.append("subsubdir");
  dir2.create(Ci.nsIFile.DIRECTORY_TYPE, 0o700);

  File.createFromNsIFile(tmpFile).then(function(file) {
    sendAsyncMessage("entries.opened", {
      data: [new Directory(tmpDir.path), file],
    });
  });
});

addMessageListener("entries.delete", function(e) {
  tmpFile.remove(true);
  tmpDir.remove(true);
  sendAsyncMessage("entries.deleted");
});
