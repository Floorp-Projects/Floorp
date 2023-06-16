/**
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

/**
 * This test is mainly to verify that init, initTemporaryStorage,
 * getUsageForPrincipal and clearStoragesForPrincipal are able to ignore
 * unknown files and directories in the storage/default directory and its
 * subdirectories.
 */
async function testSteps() {
  const principal = getPrincipal("http://example.com");

  async function testFunctionality(testFunction) {
    const modes = [
      {
        initializedStorage: false,
        initializedTemporaryStorage: false,
      },
      {
        initializedStorage: true,
        initializedTemporaryStorage: false,
      },
      {
        initializedStorage: true,
        initializedTemporaryStorage: true,
      },
    ];

    for (const mode of modes) {
      info("Clearing");

      let request = clear();
      await requestFinished(request);

      info("Installing package");

      // The profile contains unknown files and unknown directories placed
      // across the repositories, origin directories and client directories.
      // The file make_unknownFiles.js was run locally, specifically it was
      // temporarily enabled in xpcshell.ini and then executed:
      // mach test --interactive dom/quota/test/xpcshell/make_unknownFiles.js
      installPackage("unknownFiles_profile");

      if (mode.initializedStorage) {
        info("Initializing storage");

        request = init();
        await requestFinished(request);
      }

      if (mode.initializedTemporaryStorage) {
        info("Initializing temporary storage");

        request = initTemporaryStorage();
        await requestFinished(request);
      }

      info("Verifying initialization status");

      await verifyInitializationStatus(
        mode.initializedStorage,
        mode.initializedTemporaryStorage
      );

      await testFunction(
        mode.initializedStorage,
        mode.initializedTemporaryStorage
      );

      info("Clearing");

      request = clear();
      await requestFinished(request);
    }
  }

  // init and initTemporaryStorage functionality is tested in the
  // testFunctionality function as part of the multi mode testing

  info("Testing getUsageForPrincipal functionality");

  await testFunctionality(async function () {
    info("Getting origin usage");

    request = getOriginUsage(principal);
    await requestFinished(request);

    ok(
      request.result instanceof Ci.nsIQuotaOriginUsageResult,
      "The result is nsIQuotaOriginUsageResult instance"
    );
    is(request.result.usage, 115025, "Correct total usage");
    is(request.result.fileUsage, 200, "Correct file usage");
  });

  info("Testing clearStoragesForPrincipal functionality");

  await testFunctionality(async function () {
    info("Clearing origin");

    request = clearOrigin(principal, "default");
    await requestFinished(request);
  });
}
