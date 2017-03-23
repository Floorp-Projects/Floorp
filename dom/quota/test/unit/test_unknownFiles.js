/**
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

var testGenerator = testSteps();

function* testSteps()
{
  const unknownRepoFiles = [
    {
      path: "storage/persistent/foo.bar",
      dir: false
    },

    {
      path: "storage/permanent/foo.bar",
      dir: false
    }
  ];

  const exampleUrl = "http://example.com";
  const unknownRepoFile = {
    path: "storage/default/foo.bar",
    dir: false
  };

  const unknownOriginFiles = [
    {
      path: "storage/permanent/chrome/foo.bar",
      dir: false
    },

    {
      path: "storage/permanent/chrome/foo",
      dir: true
    }
  ];

  function createFile(unknownFile) {
    let file = getRelativeFile(unknownFile.path);

    if (unknownFile.dir) {
      file.create(Ci.nsIFile.DIRECTORY_TYPE, parseInt("0755", 8));
    } else {
      file.create(Ci.nsIFile.NORMAL_FILE_TYPE, parseInt("0644", 8));
    }

    return file;
  }

  let file, request;

  info("Stage 1 - Testing unknown repo files found during upgrades");

  for (let unknownFile of unknownRepoFiles) {
    info("Creating unknown file");

    file = createFile(unknownFile);

    info("Initializing");

    request = init(continueToNextStepSync)
    yield undefined;

    ok(request.resultCode == NS_OK, "Initialization succeeded");

    info("Clearing");

    request = clear(continueToNextStepSync);
    yield undefined;

    ok(request.resultCode == NS_OK, "Clearing succeeded");
  }

  info("Stage 2 - Testing unknown repo file found during tempo storage init");

  info("Initializing");

  request = init(continueToNextStepSync)
  yield undefined;

  ok(request.resultCode == NS_OK, "Initialization succeeded");

  info("Creating unknown file");

  file = createFile(unknownRepoFile);

  info("Initializing origin");

  let principal = getPrincipal(exampleUrl);
  request = initOrigin(principal, "default", continueToNextStepSync);
  yield undefined;

  ok(request.resultCode == NS_ERROR_UNEXPECTED, "Initialization failed");

  info("Clearing origin");

  request = clearOrigin(principal, "default", continueToNextStepSync);
  yield undefined;

  ok(request.resultCode == NS_OK, "Clearing succeeded");

  info("Clearing");

  request = clear(continueToNextStepSync);
  yield undefined;

  ok(request.resultCode == NS_OK, "Clearing succeeded");

  info("Stage 3 - Testing unknown origin files found during origin init");

  info("Initializing");

  request = init(continueToNextStepSync)
  yield undefined;

  ok(request.resultCode == NS_OK, "Initialization succeeded");

  for (let unknownFile of unknownOriginFiles) {
    info("Creating unknown file");

    file = createFile(unknownFile);

    info("Initializing origin");

    request = initChromeOrigin("persistent", continueToNextStepSync);
    yield undefined;

    ok(request.resultCode == NS_ERROR_UNEXPECTED, "Initialization failed");

    info("Getting usage");

    request = getCurrentUsage(continueToNextStepSync);
    yield undefined;

    ok(request.resultCode == NS_ERROR_UNEXPECTED, "Get usage failed");

    file.remove(/* recursive */ false);

    info("Getting usage");

    request = getCurrentUsage(continueToNextStepSync);
    yield undefined;

    ok(request.resultCode == NS_OK, "Get usage succeeded");

    info("Initializing origin");

    request = initChromeOrigin("persistent", continueToNextStepSync);
    yield undefined;

    ok(request.resultCode == NS_OK, "Initialization succeeded");

    file = createFile(unknownFile);

    info("Getting usage");

    request = getCurrentUsage(continueToNextStepSync);
    yield undefined;

    ok(request.resultCode == NS_OK, "Get usage succeeded");

    info("Clearing origin");

    request = clearChromeOrigin(continueToNextStepSync);
    yield undefined;

    ok(request.resultCode == NS_OK, "Clearing succeeded");
  }

  info("Clearing");

  request = clear(continueToNextStepSync);
  yield undefined;

  ok(request.resultCode == NS_OK, "Clearing succeeded");

  finishTest();
}
