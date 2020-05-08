/**
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

function* testSteps() {
  const exampleUrl = "http://example.com";
  const unknownRepoFile = {
    path: "storage/default/foo.bar",
    dir: false,
  };

  const unknownOriginFile = {
    path: "storage/permanent/chrome/foo.bar",
    dir: false,
  };

  const unknownOriginDirectory = {
    path: "storage/permanent/chrome/foo",
    dir: true,
  };

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

  info("Stage 1 - Testing unknown repo file found during tempo storage init");

  info("Initializing");

  request = init(continueToNextStepSync);
  yield undefined;

  ok(request.resultCode == NS_OK, "Initialization succeeded");

  info("Creating unknown file");

  file = createFile(unknownRepoFile);

  info("Initializing origin");

  let principal = getPrincipal(exampleUrl);
  request = initStorageAndOrigin(principal, "default", continueToNextStepSync);
  yield undefined;

  ok(
    request.resultCode == NS_OK,
    "Initialization succeeded even though there are unknown files in " +
      "repositories"
  );
  ok(request.result === true, "The origin directory was created");

  info("Clearing origin");

  request = clearOrigin(principal, "default", continueToNextStepSync);
  yield undefined;

  ok(request.resultCode == NS_OK, "Clearing succeeded");

  info("Clearing");

  request = clear(continueToNextStepSync);
  yield undefined;

  ok(request.resultCode == NS_OK, "Clearing succeeded");

  info(
    "Stage 2 - Testing unknown origin files and unknown origin directories " +
      "found during origin init"
  );

  info("Initializing");

  request = init(continueToNextStepSync);
  yield undefined;

  ok(request.resultCode == NS_OK, "Initialization succeeded");

  for (let unknownFile of [unknownOriginFile, unknownOriginDirectory]) {
    info("Creating unknown file");

    file = createFile(unknownFile);

    info("Initializing origin");

    request = initStorageAndChromeOrigin("persistent", continueToNextStepSync);
    yield undefined;

    ok(
      request.resultCode == NS_OK,
      "Initialization succeeded even though there are unknown files or " +
        "directories in origin directories"
    );
    ok(request.result === false, "The origin directory wasn't created");

    info("Getting usage");

    request = getCurrentUsage(continueToNextStepSync);
    yield undefined;

    ok(
      request.resultCode == NS_OK,
      "Get usage succeeded even though there are unknown files or directories" +
        "in origin directories"
    );
    ok(request.result, "The request result is not null");
    ok(request.result.usage === 0, "The usage was 0");
    ok(request.result.fileUsage === 0, "The fileUsage was 0");

    file.remove(/* recursive */ false);

    info("Getting usage");

    request = getCurrentUsage(continueToNextStepSync);
    yield undefined;

    ok(request.resultCode == NS_OK, "Get usage succeeded");
    ok(request.result, "The request result is not null");
    ok(request.result.usage === 0, "The usage was 0");
    ok(request.result.fileUsage === 0, "The fileUsage was 0");

    info("Initializing origin");

    request = initStorageAndChromeOrigin("persistent", continueToNextStepSync);
    yield undefined;

    ok(request.resultCode == NS_OK, "Initialization succeeded");
    ok(request.result === false, "The origin directory wasn't created");

    file = createFile(unknownFile);

    info("Getting usage");

    request = getCurrentUsage(continueToNextStepSync);
    yield undefined;

    ok(request.resultCode == NS_OK, "Get usage succeeded");
    ok(request.result, "The request result is not null");
    ok(request.result.usage === 0, "The usage was 0");
    ok(request.result.fileUsage === 0, "The fileUsage was 0");

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
