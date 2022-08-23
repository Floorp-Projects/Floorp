/**
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

add_task(async function testSteps() {
  const lsArchiveFile = "storage/ls-archive.sqlite";

  const principalInfo = {
    url: "http://example.com",
    attrs: {},
  };

  function checkStorage() {
    let principal = getPrincipal(principalInfo.url, principalInfo.attrs);
    let storage = getLocalStorage(principal);
    try {
      storage.open();
      ok(true, "Did not throw");
    } catch (ex) {
      ok(false, "Should not have thrown");
    }
  }

  info("Setting pref");

  Services.prefs.setBoolPref("dom.storage.next_gen", true);

  info("Sub test case 1 - Archive file is a directory.");

  info("Clearing");

  let request = clear();
  await requestFinished(request);

  let archiveFile = getRelativeFile(lsArchiveFile);

  archiveFile.create(Ci.nsIFile.DIRECTORY_TYPE, parseInt("0755", 8));

  checkStorage();

  info("Sub test case 2 - Corrupted archive file.");

  info("Clearing");

  request = clear();
  await requestFinished(request);

  let ostream = Cc["@mozilla.org/network/file-output-stream;1"].createInstance(
    Ci.nsIFileOutputStream
  );
  ostream.init(archiveFile, -1, parseInt("0644", 8), 0);
  ostream.write("foobar", 6);
  ostream.close();

  checkStorage();

  info("Sub test case 3 - Nonupdateable archive file.");

  info("Clearing");

  request = clear();
  await requestFinished(request);

  info("Installing package");

  // The profile contains storage.sqlite and storage/ls-archive.sqlite
  // storage/ls-archive.sqlite was taken from FF 54 to force an upgrade.
  // There's just one record in the webappsstore2 table. The record was
  // modified by renaming the origin attribute userContextId to userContextKey.
  // This triggers an error during the upgrade.
  installPackage("archive_profile");

  let fileSize = archiveFile.fileSize;
  ok(fileSize > 0, "archive file size is greater than zero");

  checkStorage();
});
