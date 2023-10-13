/* eslint-env mozilla/chrome-script */

var file;
// eslint-disable-next-line mozilla/reject-importGlobalProperties
Cu.importGlobalProperties(["File"]);

addMessageListener("file.create", function (message) {
  file = Cc["@mozilla.org/file/directory_service;1"]
    .getService(Ci.nsIProperties)
    .get("TmpD", Ci.nsIFile);
  file.append("foo.txt");
  file.createUnique(Ci.nsIFile.NORMAL_FILE_TYPE, 0o600);
  File.createFromNsIFile(file).then(function (domFile) {
    sendAsyncMessage("file.created", domFile);
  });
});

addMessageListener("file.remove", function (message) {
  file.remove(false);
  sendAsyncMessage("file.removed", {});
});
