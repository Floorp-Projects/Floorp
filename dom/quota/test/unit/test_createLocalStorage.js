/**
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

async function testSteps() {
  const webAppsStoreFile = "webappsstore.sqlite";
  const lsArchiveFile = "storage/ls-archive.sqlite";
  const lsArchiveTmpFile = "storage/ls-archive-tmp.sqlite";

  function checkArchiveFileNotExists() {
    info("Checking archive tmp file");

    let archiveTmpFile = getRelativeFile(lsArchiveTmpFile);

    let exists = archiveTmpFile.exists();
    ok(!exists, "archive tmp file doesn't exist");

    info("Checking archive file");

    let archiveFile = getRelativeFile(lsArchiveFile);

    exists = archiveFile.exists();
    ok(!exists, "archive file doesn't exist");
  }

  function checkArchiveFileExists() {
    info("Checking archive tmp file");

    let archiveTmpFile = getRelativeFile(lsArchiveTmpFile);

    let exists = archiveTmpFile.exists();
    ok(!exists, "archive tmp file doesn't exist");

    info("Checking archive file");

    let archiveFile = getRelativeFile(lsArchiveFile);

    exists = archiveFile.exists();
    ok(exists, "archive file does exist");

    info("Checking archive file size");

    let fileSize = archiveFile.fileSize;
    ok(fileSize > 0, "archive file size is greater than zero");
  }

  info("Setting pref");

  SpecialPowers.setBoolPref("dom.storage.next_gen", true);

  // Profile 1 - Nonexistent apps store file.
  info("Clearing");

  let request = clear();
  await requestFinished(request);

  let appsStoreFile = getRelativeFile(webAppsStoreFile);

  let exists = appsStoreFile.exists();
  ok(!exists, "apps store file doesn't exist");

  checkArchiveFileNotExists();

  try {
    request = init();
    await requestFinished(request);

    ok(true, "Should not have thrown");
  } catch (ex) {
    ok(false, "Should not have thrown");
  }

  checkArchiveFileExists();

  // Profile 2 - apps store file is a directory.
  info("Clearing");

  request = clear();
  await requestFinished(request);

  appsStoreFile.create(Ci.nsIFile.DIRECTORY_TYPE, parseInt("0755", 8));

  checkArchiveFileNotExists();

  try {
    request = init();
    await requestFinished(request);

    ok(true, "Should not have thrown");
  } catch (ex) {
    ok(false, "Should not have thrown");
  }

  checkArchiveFileExists();

  appsStoreFile.remove(true);

  // Profile 3 - Corrupted apps store file.
  info("Clearing");

  request = clear();
  await requestFinished(request);

  let ostream = Cc["@mozilla.org/network/file-output-stream;1"].createInstance(
    Ci.nsIFileOutputStream
  );
  ostream.init(appsStoreFile, -1, parseInt("0644", 8), 0);
  ostream.write("foobar", 6);
  ostream.close();

  checkArchiveFileNotExists();

  try {
    request = init();
    await requestFinished(request);

    ok(true, "Should not have thrown");
  } catch (ex) {
    ok(false, "Should not have thrown");
  }

  checkArchiveFileExists();

  appsStoreFile.remove(false);

  // Profile 4 - Nonupdateable apps store file.
  info("Clearing");

  request = clear();
  await requestFinished(request);

  info("Installing package");

  // The profile contains storage.sqlite and webappsstore.sqlite
  // webappstore.sqlite was taken from FF 54 to force an upgrade.
  // There's just one record in the webappsstore2 table. The record was
  // modified by renaming the origin attribute userContextId to userContextKey.
  // This triggers an error during the upgrade.
  installPackage("createLocalStorage_profile");

  let fileSize = appsStoreFile.fileSize;
  ok(fileSize > 0, "apps store file size is greater than zero");

  checkArchiveFileNotExists();

  try {
    request = init();
    await requestFinished(request);

    ok(true, "Should not have thrown");
  } catch (ex) {
    ok(false, "Should not have thrown");
  }

  checkArchiveFileExists();

  appsStoreFile.remove(false);
}
