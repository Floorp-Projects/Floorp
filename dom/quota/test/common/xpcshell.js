/**
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

loadScript("dom/quota/test/common/system.js");

function enableStorageTesting() {
  Services.prefs.setBoolPref("dom.quotaManager.testing", true);
  Services.prefs.setBoolPref("dom.simpleDB.enabled", true);
  if (Services.appinfo.OS === "WINNT") {
    Services.prefs.setBoolPref("dom.quotaManager.useDOSDevicePathSyntax", true);
  }
}

function resetStorageTesting() {
  Services.prefs.clearUserPref("dom.quotaManager.testing");
  Services.prefs.clearUserPref("dom.simpleDB.enabled");
  if (Services.appinfo.OS === "WINNT") {
    Services.prefs.clearUserPref("dom.quotaManager.useDOSDevicePathSyntax");
  }
}

function clear(callback) {
  let request = Services.qms.clear();
  request.callback = callback;

  return request;
}

function reset(callback) {
  let request = Services.qms.reset();
  request.callback = callback;

  return request;
}

function installPackage(packageRelativePath, allowFileOverwrites) {
  let currentDir = Services.dirsvc.get("CurWorkD", Ci.nsIFile);

  let packageFile = getRelativeFile(packageRelativePath + ".zip", currentDir);

  let zipReader = Cc["@mozilla.org/libjar/zip-reader;1"].createInstance(
    Ci.nsIZipReader
  );
  zipReader.open(packageFile);

  let entryNames = Array.from(zipReader.findEntries(null));
  entryNames.sort();

  for (let entryName of entryNames) {
    if (entryName.match(/^create_db\.(html|js)/)) {
      continue;
    }

    let zipentry = zipReader.getEntry(entryName);

    let file = getRelativeFile(entryName);

    if (zipentry.isDirectory) {
      if (!file.exists()) {
        file.create(Ci.nsIFile.DIRECTORY_TYPE, parseInt("0755", 8));
      }
    } else {
      if (!allowFileOverwrites && file.exists()) {
        throw new Error("File already exists!");
      }

      let istream = zipReader.getInputStream(entryName);

      var ostream = Cc[
        "@mozilla.org/network/file-output-stream;1"
      ].createInstance(Ci.nsIFileOutputStream);
      ostream.init(file, -1, parseInt("0644", 8), 0);

      let bostream = Cc[
        "@mozilla.org/network/buffered-output-stream;1"
      ].createInstance(Ci.nsIBufferedOutputStream);
      bostream.init(ostream, 32768);

      bostream.writeFrom(istream, istream.available());

      istream.close();
      bostream.close();
    }
  }

  zipReader.close();
}
