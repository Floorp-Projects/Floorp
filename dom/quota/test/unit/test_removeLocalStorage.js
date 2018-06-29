/**
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

var testGenerator = testSteps();

function* testSteps()
{
  const lsArchiveFile = "storage/ls-archive.sqlite";
  const lsArchiveTmpFile = "storage/ls-archive-tmp.sqlite";
  const lsDir = "storage/default/http+++localhost/ls";

  // Profile 1
  info("Clearing");

  clear(continueToNextStepSync);
  yield undefined;

  info("Installing package");

  installPackage("removeLocalStorage1_profile");

  info("Checking ls archive tmp file");

  let archiveTmpFile = getRelativeFile(lsArchiveTmpFile);

  let exists = archiveTmpFile.exists();
  ok(exists, "ls archive tmp file does exist");

  info("Initializing");

  let request = init(continueToNextStepSync);
  yield undefined;

  ok(request.resultCode == NS_OK, "Initialization succeeded");

  info("Checking ls archive file");

  exists = archiveTmpFile.exists();
  ok(!exists, "ls archive tmp file doesn't exist");

  // Profile 2
  info("Clearing");

  clear(continueToNextStepSync);
  yield undefined;

  info("Installing package");

  installPackage("removeLocalStorage2_profile");

  info("Checking ls archive file");

  let archiveFile = getRelativeFile(lsArchiveFile);

  exists = archiveFile.exists();
  ok(exists, "ls archive file does exist");

  info("Checking ls dir");

  let dir = getRelativeFile(lsDir);

  exists = dir.exists();
  ok(exists, "ls directory does exist");

  info("Initializing");

  request = init(continueToNextStepSync);
  yield undefined;

  ok(request.resultCode == NS_OK, "Initialization succeeded");

  info("Checking ls archive file");

  exists = archiveFile.exists();
  ok(!exists, "ls archive file doesn't exist");

  info("Checking ls dir");

  exists = dir.exists();
  ok(!exists, "ls directory doesn't exist");

  finishTest();
}
