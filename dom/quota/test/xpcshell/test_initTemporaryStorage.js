/**
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

/**
 * This test is mainly to verify initTemporaryStorage() does call
 * QuotaManager::EnsureTemporaryStorageIsInitialized() which does various
 * things, for example,
 *  - it restores the directory metadata if it's broken or missing.
 *  - it isn't blocked by a cache directory in an origin that ends with period.
 */

async function testSteps() {
  const defaultRepositoryPath = "storage/default";
  const originDirPaths = [
    `${defaultRepositoryPath}/https+++foo.example.com`,
    // Bug 1647316: We should have a test that checks all possible combinations
    // that we can get during cache origin initialization. Once we have a test
    // for that, we can revert this change.
    `${defaultRepositoryPath}/https+++example.com.`,
  ];

  const metadataFileName = ".metadata-v2";

  info("Initializing");

  let request = init();
  await requestFinished(request);

  info("Verifying initialization status");

  await verifyInitializationStatus(true, false);

  info("Creating an empty directory");

  let originDirs = [];
  for (let originDirPath of originDirPaths) {
    let originDir = getRelativeFile(originDirPath);
    originDir.create(Ci.nsIFile.DIRECTORY_TYPE, 0o755);
    originDirs.push(originDir);
  }

  info("Creating an empty cache directory for origin that ends with period");

  let originDirPathEndsWithPeriod = originDirPaths.find(path =>
    path.endsWith(".")
  );
  let cacheDir = getRelativeFile(`${originDirPathEndsWithPeriod}/cache`);
  cacheDir.create(Ci.nsIFile.DIRECTORY_TYPE, 0o755);

  info("Initializing the temporary storage");

  request = initTemporaryStorage();
  await requestFinished(request);

  info(
    "Verifying directory metadata was restored after calling " +
      "initTemporaryStorage()"
  );

  for (let originDir of originDirs) {
    let metadataFile = originDir.clone();
    metadataFile.append(metadataFileName);

    ok(metadataFile.exists(), "Directory metadata file does exist");
  }

  info("Verifying initialization status");

  await verifyInitializationStatus(true, true);
}
