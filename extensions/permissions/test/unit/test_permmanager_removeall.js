/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

add_task(async function test() {
  Services.prefs.setCharPref("permissions.manager.defaultsUrl", "");
  // setup a profile directory
  var dir = do_get_profile();

  // We need to execute a pm method to be sure that the DB is fully
  // initialized.
  var pm = Services.perms;
  Assert.ok(pm.all.length === 0);

  Services.obs.notifyObservers(null, "testonly-reload-permissions-from-disk");

  // get the db file
  var file = dir.clone();
  file.append("permissions.sqlite");

  Assert.ok(file.exists());

  // corrupt the file
  var ostream = Cc["@mozilla.org/network/file-output-stream;1"].createInstance(
    Ci.nsIFileOutputStream
  );
  ostream.init(file, 0x02, 0o666, 0);
  var conv = Cc["@mozilla.org/intl/converter-output-stream;1"].createInstance(
    Ci.nsIConverterOutputStream
  );
  conv.init(ostream, "UTF-8");
  for (var i = 0; i < file.fileSize; ++i) {
    conv.writeString("a");
  }
  conv.close();

  // prepare an empty hostperm.1 file so that it can be used for importing
  var hostperm = dir.clone();
  hostperm.append("hostperm.1");
  ostream.init(hostperm, 0x02 | 0x08, 0o666, 0);
  ostream.close();

  // remove all should not throw
  pm.removeAll();
});
