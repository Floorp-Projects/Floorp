/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

function run_test() {
  // setup a profile directory
  var dir = do_get_profile();

  // initialize the permission manager service
  var pm = Cc["@mozilla.org/permissionmanager;1"].
           getService(Ci.nsIPermissionManager);

  // get the db file
  var file = dir.clone();
  file.append("permissions.sqlite");
  do_check_true(file.exists());

  // corrupt the file
  var ostream = Cc["@mozilla.org/network/file-output-stream;1"].
                createInstance(Ci.nsIFileOutputStream);
  ostream.init(file, 0x02, 0666, 0);
  var conv = Cc["@mozilla.org/intl/converter-output-stream;1"].
             createInstance(Ci.nsIConverterOutputStream);
  conv.init(ostream, "UTF-8", 0, 0);
  for (var i = 0; i < file.fileSize; ++i)
    conv.writeString("a");
  conv.close();

  // prepare an empty hostperm.1 file so that it can be used for importing
  var hostperm = dir.clone();
  hostperm.append("hostperm.1");
  ostream.init(hostperm, 0x02 | 0x08, 0666, 0);
  ostream.close();

  // remove all should not throw
  pm.removeAll();
}
