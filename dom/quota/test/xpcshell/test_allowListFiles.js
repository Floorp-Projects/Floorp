/**
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

/**
 * This test is mainly to verify thoes unexpected files are in the allow list of
 * QuotaManager. They aren't expected in the repository but if there are,
 * QuotaManager shouldn't fail to initialize an origin and getting usage, though
 * those files aren't managed by QuotaManager.
 */

async function testSteps() {
  const allowListFiles = [
    ".dot-file",
    "desktop.ini",
    "Desktop.ini",
    "Thumbs.db",
    "thumbs.db",
  ];

  for (let allowListFile of allowListFiles) {
    info("Testing " + allowListFile + " in the repository");

    info("Creating unknown file");

    for (let dir of ["persistenceType dir", "origin dir"]) {
      let dirPath =
        dir == "persistenceType dir"
          ? "storage/default/"
          : "storage/default/http+++example.com/";
      let file = getRelativeFile(dirPath + allowListFile);
      file.create(Ci.nsIFile.NORMAL_FILE_TYPE, parseInt("0644", 8));
    }

    info("Initializing an origin");

    let request = initStorageAndOrigin(
      getPrincipal("http://example.com"),
      "default"
    );
    await requestFinished(request);

    info("Resetting");

    request = reset();
    await requestFinished(request);

    info("Getting usage");

    request = getCurrentUsage(continueToNextStepSync);
    await requestFinished(request);

    info("Clearing");

    request = clear();
    await requestFinished(request);
  }
}
