/**
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

var testGenerator = testSteps();

function* testSteps()
{
  info("Clearing");

  clear(continueToNextStepSync);
  yield undefined;

  info("Getting usage");

  getUsage(grabUsageAndContinueHandler);
  let usage = yield undefined;

  ok(usage == 0, "Usage is zero");

  info("Installing package");

  // The profile contains just one empty IndexedDB database. The file
  // create_db.js in the package was run locally, specifically it was
  // temporarily added to xpcshell.ini and then executed:
  // mach xpcshell-test --interactive dom/quota/test/unit/create_db.js
  installPackage("basics_profile");

  info("Getting usage");

  getUsage(grabUsageAndContinueHandler);
  usage = yield undefined;

  ok(usage > 0, "Usage is not zero");

  finishTest();
}
