// ensure that the directory we are writing into is empty
try {
  var Cc = Components.classes;
  var Ci = Components.interfaces;
  var directoryService = Cc["@mozilla.org/file/directory_service;1"].getService(Ci.nsIProperties);
  var f = directoryService.get("TmpD", Ci.nsIFile);
  f.appendRelativePath("device-storage-testing");
  f.remove(true);
} catch(e) {}

sendAsyncMessage('directory-removed', {});
