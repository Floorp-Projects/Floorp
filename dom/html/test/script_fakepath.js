/* eslint-env mozilla/chrome-script */

// eslint-disable-next-line mozilla/reject-importGlobalProperties
Cu.importGlobalProperties(["File"]);

addMessageListener("file.open", function (e) {
  var tmpFile = Cc["@mozilla.org/file/directory_service;1"]
    .getService(Ci.nsIDirectoryService)
    .QueryInterface(Ci.nsIProperties)
    .get("ProfD", Ci.nsIFile);
  tmpFile.append("prefs.js");

  File.createFromNsIFile(tmpFile).then(file => {
    sendAsyncMessage("file.opened", { data: [file] });
  });
});
