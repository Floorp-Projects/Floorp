/**
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

/**
 * This test is mainly to verify that initStorageAndOrigin, getUsageForPrincipal
 * and clearStoragesForPrincipal are able to ignore unknown files and
 * directories in the storage/default directory and its subdirectories.
 */

function* testSteps() {
  const exampleUrl = "http://example.com";
  const unknownRepoFile = {
    path: "storage/default/foo.bar",
    dir: false,
  };

  const unknownOriginFile = {
    path: "storage/default/http+++foobar.com/foo.bar",
    dir: false,
  };

  const unknownOriginDirectory = {
    path: "storage/default/http+++foobar.com/foo",
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

  info("Initializing");

  request = init(continueToNextStepSync);
  yield undefined;

  ok(request.resultCode == NS_OK, "Initialization succeeded");

  for (let unknownFile of [
    unknownRepoFile,
    unknownOriginFile,
    unknownOriginDirectory,
  ]) {
    info("Creating unknown file");

    file = createFile(unknownFile);

    info("Initializing origin");

    const principal = getPrincipal(exampleUrl);
    request = initStorageAndOrigin(
      principal,
      "default",
      continueToNextStepSync
    );
    yield undefined;

    ok(
      request.resultCode == NS_OK,
      "Initialization succeeded even though there are unknown files or " +
        "directories"
    );
    ok(request.result === true, "The origin directory was created");

    info("Getting origin usage");

    request = getOriginUsage(principal);
    requestFinished(request).then(continueToNextStepSync);
    yield undefined;

    ok(request.result, "The request result is not null");
    ok(request.result.usage === 0, "The usage was 0");
    ok(request.result.fileUsage === 0, "The fileUsage was 0");

    file.remove(/* recursive */ false);

    info("Getting origin usage");

    request = getOriginUsage(principal);
    requestFinished(request).then(continueToNextStepSync);
    yield undefined;

    ok(request.result, "The request result is not null");
    ok(request.result.usage === 0, "The usage was 0");
    ok(request.result.fileUsage === 0, "The fileUsage was 0");

    info("Initializing origin");

    request = initStorageAndOrigin(
      principal,
      "default",
      continueToNextStepSync
    );
    yield undefined;

    ok(request.resultCode == NS_OK, "Initialization succeeded");
    ok(request.result === false, "The origin directory wasn't created");

    file = createFile(unknownFile);

    info("Getting origin usage");

    request = getOriginUsage(principal);
    requestFinished(request).then(continueToNextStepSync);
    yield undefined;

    ok(request.result, "The request result is not null");
    ok(request.result.usage === 0, "The usage was 0");
    ok(request.result.fileUsage === 0, "The fileUsage was 0");

    info("Clearing origin");

    request = clearOrigin(principal, "default", continueToNextStepSync);
    yield undefined;

    ok(request.resultCode == NS_OK, "Clearing succeeded");
  }

  info("Clearing");

  request = clear(continueToNextStepSync);
  yield undefined;

  ok(request.resultCode == NS_OK, "Clearing succeeded");

  finishTest();
}
