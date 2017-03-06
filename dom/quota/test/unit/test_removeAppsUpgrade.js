/**
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

var testGenerator = testSteps();

function* testSteps()
{
  const origins = [
    {
      path: "storage/default/http+++www.mozilla.org",
      obsolete: false
    },

    {
      path: "storage/default/app+++system.gaiamobile.org^appId=1007",
      obsolete: true
    },

    {
      path: "storage/default/https+++developer.cdn.mozilla.net^appId=1007&inBrowser=1",
      obsolete: true
    }
  ];

  info("Clearing");

  clear(continueToNextStepSync);
  yield undefined;

  info("Installing package");

  installPackage("removeAppsUpgrade_profile");

  info("Checking origin directories");

  for (let origin of origins) {
    let originDir = getRelativeFile(origin.path);

    let exists = originDir.exists();
    ok(exists, "Origin directory does exist");
  }

  info("Initializing");

  let request = init(continueToNextStepSync);
  yield undefined;

  ok(request.resultCode == NS_OK, "Initialization succeeded");

  info("Checking origin directories");

  for (let origin of origins) {
    let originDir = getRelativeFile(origin.path);

    let exists = originDir.exists();
    if (origin.obsolete) {
      ok(!exists, "Origin directory doesn't exist");
    } else {
      ok(exists, "Origin directory does exist");
    }
  }

  finishTest();
}
