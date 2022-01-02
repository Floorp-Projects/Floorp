Cu.importGlobalProperties(["File"]);

var file;

addMessageListener("file.open", function(stem) {
  try {
    if (!file) {
      file = Cc["@mozilla.org/file/directory_service;1"]
        .getService(Ci.nsIProperties)
        .get("TmpD", Ci.nsIFile);
      file.append(stem);
      file.createUnique(Ci.nsIFile.NORMAL_FILE_TYPE, 0o600);
    }

    File.createFromNsIFile(file).then(function(domFile) {
      sendAsyncMessage("file.opened", {
        fullPath: file.path,
        leafName: file.leafName,
        domFile,
      });
    });
  } catch (e) {
    sendAsyncMessage("fail", e.toString());
  }
});

addMessageListener("file.remove", function() {
  try {
    file.remove(/* recursive: */ false);
    file = undefined;
    sendAsyncMessage("file.removed", null);
  } catch (e) {
    sendAsyncMessage("fail", e.toString());
  }
});
