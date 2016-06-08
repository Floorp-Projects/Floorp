var { classes: Cc, interfaces: Ci, utils: Cu } = Components;
Cu.importGlobalProperties(["File", "Directory"]);

addMessageListener("entries.open", function (e) {
  var tmpFile = Cc["@mozilla.org/file/directory_service;1"]
                  .getService(Ci.nsIDirectoryService)
                  .QueryInterface(Ci.nsIProperties)
                  .get('TmpD', Ci.nsIFile)
  tmpFile.append('file.txt');
  tmpFile.createUnique(Components.interfaces.nsIFile.FILE_TYPE, 0o600);

  var tmpDir = Cc["@mozilla.org/file/directory_service;1"]
                  .getService(Ci.nsIDirectoryService)
                  .QueryInterface(Ci.nsIProperties)
                  .get('TmpD', Ci.nsIFile)

  tmpDir.append('dir-test');
  tmpDir.createUnique(Components.interfaces.nsIFile.DIRECTORY_TYPE, 0o700);

  var file1 = tmpDir.clone();
  file1.append('foo.txt');
  file1.create(Components.interfaces.nsIFile.NORMAL_FILE_TYPE, 0o600);

  var dir1 = tmpDir.clone();
  dir1.append('subdir');
  dir1.create(Components.interfaces.nsIFile.DIRECTORY_TYPE, 0o700);

  var file2 = dir1.clone();
  file2.append('bar.txt');
  file2.create(Components.interfaces.nsIFile.NORMAL_FILE_TYPE, 0o600);

  var dir2 = dir1.clone();
  dir2.append('subsubdir');
  dir2.create(Components.interfaces.nsIFile.DIRECTORY_TYPE, 0o700);

  sendAsyncMessage("entries.opened", {
    data: [ new Directory(tmpDir.path), new File(tmpFile) ]
  });
});
