/**
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

/**
 * This test is mainly to verify that initTemporaryStorage,
 * initStorageAndOrigin, getUsageForPrincipal and clearStoragesForPrincipal are
 * able to ignore unknown files and directories in the storage/default
 * directory and its subdirectories.
 */
async function testSteps() {
  const principal = getPrincipal("http://example.com");

  async function testFunctionality(testFunction) {
    const originDirectory = getRelativeFile(
      "storage/default/http+++example.com"
    );

    const unknownFiles = [
      {
        path: "storage/default/foo.bar",
        dir: false,
      },

      {
        path: "storage/default/http+++foobar.com/foo.bar",
        dir: false,
      },

      {
        path: "storage/default/http+++foobar.com/foo",
        dir: true,
      },

      // TODO: Fix IndexedDB to ignore unknown files as well
      /*
      {
        path: "storage/default/http+++foobar.com/idb/foo.bar",
        dir: false,
      },
      */

      {
        path: "storage/default/http+++foobar.com/cache/foo.bar",
        dir: false,
      },

      {
        path: "storage/default/http+++foobar.com/sdb/foo.bar",
        dir: false,
      },

      {
        path: "storage/default/http+++foobar.com/ls/foo.bar",
        dir: false,
      },
    ];

    for (const createOriginDirectory of [false, true]) {
      info("Initializing storage");

      let request = init();
      await requestFinished(request);

      if (createOriginDirectory) {
        info("Creating origin directory");

        originDirectory.create(Ci.nsIFile.DIRECTORY_TYPE, parseInt("0755", 8));
      }

      info("Creating unknown files");

      for (const unknownFile of unknownFiles) {
        const file = getRelativeFile(unknownFile.path);

        if (unknownFile.dir) {
          file.create(Ci.nsIFile.DIRECTORY_TYPE, parseInt("0755", 8));
        } else {
          file.create(Ci.nsIFile.NORMAL_FILE_TYPE, parseInt("0644", 8));
        }
      }

      await testFunction(createOriginDirectory);

      info("Clearing");

      request = clear();
      await requestFinished(request);
    }
  }

  info("Testing initTemporaryStorage functionality");

  await testFunctionality(async function() {
    info("Initializing temporary storage");

    request = initTemporaryStorage();
    await requestFinished(request);
  });

  info("Testing initStorageAndOrigin functionality");

  await testFunctionality(async function(createdOriginDirectory) {
    info("Initializing origin");

    request = initStorageAndOrigin(principal, "default");
    await requestFinished(request);

    if (createdOriginDirectory) {
      ok(request.result === false, "The origin directory was not created");
    } else {
      ok(request.result === true, "The origin directory was created");
    }
  });

  info("Testing getUsageForPrincipal functionality");

  await testFunctionality(async function() {
    info("Getting origin usage");

    request = getOriginUsage(principal);
    await requestFinished(request);

    ok(
      request.result instanceof Ci.nsIQuotaOriginUsageResult,
      "The result is nsIQuotaOriginUsageResult instance"
    );
    ok(request.result.usage === 0, "The usage is 0");
    ok(request.result.fileUsage === 0, "The fileUsage is 0");
  });

  info("Testing clearStoragesForPrincipal functionality");

  await testFunctionality(async function() {
    info("Clearing origin");

    request = clearOrigin(principal, "default");
    await requestFinished(request);
  });
}
