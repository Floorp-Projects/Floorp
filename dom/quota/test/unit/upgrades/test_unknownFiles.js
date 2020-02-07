/**
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

var testGenerator = testSteps();

function* testSteps() {
  const unknownRepositoryFiles = [
    {
      path: "storage/persistent/foo.bar",
      dir: false,
    },

    {
      path: "storage/permanent/foo.bar",
      dir: false,
    },

    {
      path: "storage/default/invalid+++example.com",
      dir: true,
    },
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

  info(
    "Testing unknown repository files and unknown repository directories " +
      "found during upgrades"
  );

  for (let unknownFile of unknownRepositoryFiles) {
    info("Creating unknown file");

    file = createFile(unknownFile);

    info("Initializing");

    request = init(continueToNextStepSync);
    yield undefined;

    ok(
      request.resultCode == NS_OK,
      "Initialization succeeded even though there are unknown files or " +
        "directories in repositories"
    );

    info("Clearing");

    request = clear(continueToNextStepSync);
    yield undefined;

    ok(request.resultCode == NS_OK, "Clearing succeeded");
  }

  finishTest();
}
