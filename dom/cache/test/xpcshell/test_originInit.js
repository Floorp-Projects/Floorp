/**
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

async function testSteps() {
  // ToDo: Replace storage and default with a getter function once we expose the
  // filenames of them to a IDL file.
  const basePath = `${storageDirName}/${defaultPersistenceDirName}/`;
  const principal = getPrincipal("https://example.com");
  const originDirName = "https+++example.com";

  // ToDo: Replace caches.sqlite with a getter function once we expose the
  // filename of it to a IDL file.
  const cachesDatabase = getRelativeFile(
    `${basePath}/${originDirName}/${cacheClientDirName}/caches.sqlite`
  );
  // ToDo: Replace .padding with a getter function once we expose the filename
  // of it to a IDL file.
  const paddingFile = getRelativeFile(
    `${basePath}/${originDirName}/${cacheClientDirName}/.padding`
  );
  // ToDo: Replace .padding-tmp with a getter function once we expose the
  // filename of it to a IDL file.
  const paddingTempFile = getRelativeFile(
    `${basePath}/${originDirName}/${cacheClientDirName}/.padding-tmp`
  );

  async function createTestOrigin() {
    async function sandboxScript() {
      const cache = await caches.open("myCache");
      const request = new Request("https://example.com/index.html");
      const response = new Response("hello world");
      await cache.put(request, response);
    }

    const sandbox = new Cu.Sandbox(principal, {
      wantGlobalProperties: ["caches", "fetch"],
    });

    const promise = new Promise(function(resolve, reject) {
      sandbox.resolve = resolve;
      sandbox.reject = reject;
    });

    Cu.evalInSandbox(
      sandboxScript.toSource() + " sandboxScript().then(resolve, reject);",
      sandbox
    );
    await promise;

    let request = reset();
    await requestFinished(request);
  }

  function removeFile(file) {
    file.remove(false);
  }

  function createEmptyFile(file) {
    file.create(Ci.nsIFile.NORMAL_FILE_TYPE, 0o644);
  }

  function checkFiles(
    expectCachesDatabase,
    expectPaddingFile,
    expectTempPaddingFile
  ) {
    let exists = cachesDatabase.exists();
    if (expectCachesDatabase) {
      ok(exists, "caches.sqlite does exist");
    } else {
      ok(!exists, "caches.sqlite doesn't exist");
    }

    exists = paddingFile.exists();
    if (expectPaddingFile) {
      ok(exists, ".padding does exist");
    } else {
      ok(!exists, ".padding doesn't exist");
    }

    exists = paddingTempFile.exists();
    if (expectTempPaddingFile) {
      ok(exists, ".padding-tmp does exist");
    } else {
      ok(!exists, ".padding-tmp doesn't exist");
    }
  }

  async function testInitFunctionality(
    hasCachesDatabase,
    hasPaddingFile,
    hasTempPaddingFile
  ) {
    info(
      `Testing init cache directory when caches.sqlite ` +
        `${hasCachesDatabase ? "exists" : "doesn't exist"}, .padding ` +
        `${hasPaddingFile ? "exists" : "doesn't exist"}, .padding-tmp ` +
        `${hasTempPaddingFile ? "exists" : "doesn't exist"}`
    );

    await createTestOrigin();

    checkFiles(true, true, false);

    if (hasTempPaddingFile) {
      createEmptyFile(paddingTempFile);
    }

    if (!hasPaddingFile) {
      removeFile(paddingFile);
    }

    if (!hasCachesDatabase) {
      removeFile(cachesDatabase);
    }

    let request = initStorageAndOrigin(principal);
    await requestFinished(request);

    // After the origin is initialized, ".padding-tmp" should have always been
    // removed and ".padding" should only exist when "caches.sqlite" exists.
    checkFiles(hasCachesDatabase, hasCachesDatabase, false);

    request = clearOrigin(principal);
    await requestFinished(request);
  }

  await testInitFunctionality(false, false, false);
  // ToDo: .padding-tmp will be removed when the first cache operation is
  // executed, but we should remove it during InitOrigin in this case.
  //await testInitFunctionality(false, false, true);
  // ToDo: .padding should be removed when there is no caches.sqlite.
  //await testInitFunctionality(false, true, false);
  // ToDo: In this case, padding size should be counted as zero because there is
  // no database, but we don't.
  //await testInitFunctionality(false, true, true);
  await testInitFunctionality(true, false, false);
  await testInitFunctionality(true, false, true);
  await testInitFunctionality(true, true, false);
  await testInitFunctionality(true, true, true);
}
