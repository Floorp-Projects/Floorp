add_task(async function() {
  const {Services} = ChromeUtils.import("resource://gre/modules/Services.jsm");

  do_get_profile();

  let existingFile = Services.dirsvc.QueryInterface(Ci.nsIProperties).get("ProfD", Ci.nsIFile);
  existingFile.append("exists.js");
  existingFile.createUnique(Ci.nsIFile.NORMAL_FILE_TYPE, 0o600);

  var outStream = Cc["@mozilla.org/network/file-output-stream;1"]
                      .createInstance(Ci.nsIFileOutputStream);
  outStream.init(existingFile, 0x02 | 0x08 | 0x20, // write, create, truncate
                 0666, 0);

  var fileData = "Hello World!";
  outStream.write(fileData, fileData.length);
  outStream.close();

  ok(existingFile.exists(), "exists.js exists");

  let unknownFile = Services.dirsvc.QueryInterface(Ci.nsIProperties).get("TmpD", Ci.nsIFile);
  unknownFile.append("wow.txt");

  ok(!unknownFile.exists(), unknownFile.path + " doesn't exist");

  let a = await File.createFromNsIFile(existingFile, { existenceCheck: false });
  ok(a.size != 0, "The size is correctly set");

  let b = await File.createFromNsIFile(unknownFile, { existenceCheck: false });
  ok(b.size == 0, "The size is 0 for unknown file");

  let c = await File.createFromNsIFile(existingFile, { existenceCheck: true });
  ok(c.size != 0, "The size is correctly set");

  let d = await File.createFromNsIFile(unknownFile, { existenceCheck: true }).then(_ => true, _ => false);
  ok(d === false, "Exception thrown");

  existingFile.remove(true);
  ok(!existingFile.exists(), "exists.js doesn't exist anymore");
});
