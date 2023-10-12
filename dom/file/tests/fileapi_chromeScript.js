/* eslint-env mozilla/chrome-script */

// eslint-disable-next-line mozilla/reject-importGlobalProperties
Cu.importGlobalProperties(["File"]);

function createFileWithData(fileData) {
  var willDelete = fileData === null;

  var dirSvc = Cc["@mozilla.org/file/directory_service;1"].getService(
    Ci.nsIProperties
  );

  var testFile = dirSvc.get("ProfD", Ci.nsIFile);
  testFile.append("fileAPItestfile");
  testFile.createUnique(Ci.nsIFile.NORMAL_FILE_TYPE, 0o600);

  var outStream = Cc[
    "@mozilla.org/network/file-output-stream;1"
  ].createInstance(Ci.nsIFileOutputStream);
  outStream.init(
    testFile,
    0x02 | 0x08 | 0x20, // write, create, truncate
    0o666,
    0
  );
  if (willDelete) {
    fileData = "some irrelevant test data\n";
  }
  outStream.write(fileData, fileData.length);
  outStream.close();
  return File.createFromNsIFile(testFile).then(domFile => {
    if (willDelete) {
      testFile.remove(/* recursive: */ false);
    }

    return domFile;
  });
}

addMessageListener("files.open", function (message) {
  let promises = [];
  let list = [];

  for (let fileData of message) {
    promises.push(
      createFileWithData(fileData).then(domFile => {
        list.push(domFile);
      })
    );
  }

  Promise.all(promises).then(() => {
    sendAsyncMessage("files.opened", list);
  });
});
