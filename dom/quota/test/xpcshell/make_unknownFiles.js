/**
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

loadScript("dom/quota/test/common/file.js");

async function testSteps() {
  const principal = getPrincipal("http://example.com");

  const repoRelativePath = "storage/default";
  const originRelativePath = `${repoRelativePath}/http+++example.com`;

  let unknownFileCounter = 1;
  let unknownDirCounter = 1;

  function createUnknownFileIn(dirRelativePath, recursive) {
    const dir = getRelativeFile(dirRelativePath);

    let file = dir.clone();
    file.append("foo" + unknownFileCounter + ".bar");

    const ostream = Cc[
      "@mozilla.org/network/file-output-stream;1"
    ].createInstance(Ci.nsIFileOutputStream);

    ostream.init(file, -1, parseInt("0644", 8), 0);

    ostream.write("x".repeat(unknownFileCounter), unknownFileCounter);

    ostream.close();

    unknownFileCounter++;

    if (recursive) {
      const entries = dir.directoryEntries;
      while ((file = entries.nextFile)) {
        if (file.isDirectory()) {
          createUnknownFileIn(dirRelativePath + "/" + file.leafName);
        }
      }
    }
  }

  function createUnknownDirectoryIn(dirRelativePath) {
    createUnknownFileIn(dirRelativePath + "/foo" + unknownDirCounter++);
  }

  // storage.sqlite and storage/ls-archive.sqlite
  {
    const request = init();
    await requestFinished(request);
  }

  // Unknown file in the repository
  {
    createUnknownFileIn(repoRelativePath);
  }

  // Unknown file and unknown directory in the origin directory
  {
    let request = init();
    await requestFinished(request);

    request = initTemporaryStorage();
    await requestFinished(request);

    request = initTemporaryOrigin("default", principal);
    await requestFinished(request);

    ok(request.result === true, "The origin directory was created");

    createUnknownFileIn(originRelativePath);
    createUnknownDirectoryIn(originRelativePath);
  }

  // Unknown files in idb client directory and its subdirectories and unknown
  // directory in .files directory
  {
    const request = indexedDB.openForPrincipal(principal, "myIndexedDB");
    await openDBRequestUpgradeNeeded(request);

    const database = request.result;

    const objectStore = database.createObjectStore("Blobs", {});

    objectStore.add(getNullBlob(200), 42);

    await openDBRequestSucceeded(request);

    database.close();

    createUnknownFileIn(`${originRelativePath}/idb`);
    createUnknownFileIn(
      `${originRelativePath}/idb/2320029346mByDIdnedxe.files`
    );
    createUnknownDirectoryIn(
      `${originRelativePath}/idb/2320029346mByDIdnedxe.files`
    );
    createUnknownFileIn(
      `${originRelativePath}/idb/2320029346mByDIdnedxe.files/journals`
    );
  }

  // Unknown files in cache client directory and its subdirectories
  {
    async function sandboxScript() {
      const cache = await caches.open("myCache");
      const request = new Request("http://example.com/index.html");
      const response = new Response("hello world");
      await cache.put(request, response);
    }

    const sandbox = new Cu.Sandbox(principal, {
      wantGlobalProperties: ["caches", "fetch"],
    });

    const promise = new Promise(function (resolve, reject) {
      sandbox.resolve = resolve;
      sandbox.reject = reject;
    });

    Cu.evalInSandbox(
      sandboxScript.toSource() + " sandboxScript().then(resolve, reject);",
      sandbox
    );
    await promise;

    createUnknownFileIn(`${originRelativePath}/cache`);
    createUnknownFileIn(
      `${originRelativePath}/cache/morgue`,
      /* recursive */ true
    );
  }

  // Unknown file and unknown directory in sdb client directory
  {
    const database = getSimpleDatabase(principal);

    let request = database.open("mySimpleDB");
    await requestFinished(request);

    request = database.write(getBuffer(100));
    await requestFinished(request);

    request = database.close();
    await requestFinished(request);

    createUnknownFileIn(`${originRelativePath}/sdb`);
    createUnknownDirectoryIn(`${originRelativePath}/sdb`);
  }

  // Unknown file and unknown directory in ls client directory
  {
    Services.prefs.setBoolPref("dom.storage.testing", true);
    Services.prefs.setBoolPref("dom.storage.client_validation", false);

    const storage = Services.domStorageManager.createStorage(
      null,
      principal,
      principal,
      ""
    );

    storage.setItem("foo", "bar");

    storage.close();

    createUnknownFileIn(`${originRelativePath}/ls`);
    createUnknownDirectoryIn(`${originRelativePath}/ls`);
  }
}
