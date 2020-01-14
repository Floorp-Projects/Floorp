/**
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

var testGenerator = testSteps();

function* testSteps() {
  const morgueFile = "storage/default/http+++example.com/morgue";

  info("Clearing");

  clear(continueToNextStepSync);
  yield undefined;

  info("Installing package");

  installPackage("morgueCleanup_profile");

  info("Checking morgue file");

  let file = getRelativeFile(morgueFile);

  let exists = file.exists();
  ok(exists, "Morgue file does exist");

  info("Initializing");

  let request = init(continueToNextStepSync);
  yield undefined;

  ok(request.resultCode == NS_OK, "Initialization succeeded");

  info("Checking morgue file");

  exists = file.exists();
  ok(!exists, "Morgue file doesn't exist");

  finishTest();
}
