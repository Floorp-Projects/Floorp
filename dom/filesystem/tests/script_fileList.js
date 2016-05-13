var { classes: Cc, interfaces: Ci, utils: Cu } = Components;
Cu.importGlobalProperties(["File"]);

function createProfDFile() {
  return Cc["@mozilla.org/file/directory_service;1"]
           .getService(Ci.nsIDirectoryService)
           .QueryInterface(Ci.nsIProperties)
           .get('ProfD', Ci.nsIFile);
}

function createRootFile() {
  var testFile = createProfDFile();

  // Let's go back to the root of the FileSystem
  while (true) {
    var parent = testFile.parent;
    if (!parent) {
      break;
    }

    testFile = parent;
  }

  return testFile;
}

function createTestFile() {
  var tmpFile = Cc["@mozilla.org/file/directory_service;1"]
                  .getService(Ci.nsIDirectoryService)
                  .QueryInterface(Ci.nsIProperties)
                  .get('TmpD', Ci.nsIFile)
  tmpFile.append('dir-test');
  tmpFile.createUnique(Components.interfaces.nsIFile.DIRECTORY_TYPE, 0o700);

  var file1 = tmpFile.clone();
  file1.append('foo.txt');
  file1.create(Components.interfaces.nsIFile.NORMAL_FILE_TYPE, 0o600);

  var dir = tmpFile.clone();
  dir.append('subdir');
  dir.create(Components.interfaces.nsIFile.DIRECTORY_TYPE, 0o700);

  var file2 = dir.clone();
  file2.append('bar.txt');
  file2.create(Components.interfaces.nsIFile.NORMAL_FILE_TYPE, 0o600);

  return tmpFile;
}

addMessageListener("dir.open", function (e) {
  var testFile;

  switch (e.path) {
    case 'ProfD':
      testFile = createProfDFile();
      break;

    case 'root':
      testFile = createRootFile();
      break;

    case 'test':
      testFile = createTestFile();
      break;
  }

  sendAsyncMessage("dir.opened", {
    dir: testFile.path
  });
});

addMessageListener("file.open", function (e) {
  var testFile = Cc["@mozilla.org/file/directory_service;1"]
                   .getService(Ci.nsIDirectoryService)
                   .QueryInterface(Ci.nsIProperties)
                   .get("ProfD", Ci.nsIFile);
  testFile.append("prefs.js");

  sendAsyncMessage("file.opened", {
    file: new File(testFile)
  });
});
