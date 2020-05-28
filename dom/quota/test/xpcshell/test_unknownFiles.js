/**
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

/**
 * This test is mainly to verify that initStorageAndOrigin, getUsageForPrincipal
 * and clearStoragesForPrincipal are able to ignore unknown files and
 * directories in the storage/default directory and its subdirectories.
 */

async function testSteps() {
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

  let request;

  info("Initializing storage");

  request = init();
  await requestFinished(request);

  for (let unknownFile of [
    unknownRepoFile,
    unknownOriginFile,
    unknownOriginDirectory,
  ]) {
    info("Creating unknown file");

    createFile(unknownFile);

    info("Initializing origin");

    const principal = getPrincipal(exampleUrl);
    request = initStorageAndOrigin(principal, "default");
    await requestFinished(request);

    ok(request.result === true, "The origin directory was created");

    info("Getting origin usage");

    request = getOriginUsage(principal);
    await requestFinished(request);

    ok(request.result, "The request result is not null");
    ok(request.result.usage === 0, "The usage was 0");
    ok(request.result.fileUsage === 0, "The fileUsage was 0");

    info("Clearing origin");

    request = clearOrigin(principal, "default");
    await requestFinished(request);
  }

  info("Clearing");

  request = clear();
  await requestFinished(request);
}
