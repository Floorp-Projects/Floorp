/**
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

async function testSteps() {
  const metadataFile = getRelativeFile(
    "storage/default/https+++foo.example.com/.metadata-v2"
  );

  function getLastAccessTime() {
    let fileInputStream = Cc[
      "@mozilla.org/network/file-input-stream;1"
    ].createInstance(Ci.nsIFileInputStream);

    fileInputStream.init(metadataFile, -1, -1, 0);

    let binaryInputStream = Cc[
      "@mozilla.org/binaryinputstream;1"
    ].createInstance(Ci.nsIBinaryInputStream);

    binaryInputStream.setInputStream(fileInputStream);

    let lastAccessTime = BigInt.asIntN(64, BigInt(binaryInputStream.read64()));

    binaryInputStream.close();

    return lastAccessTime;
  }

  info("Clearing");

  let request = clear();
  await requestFinished(request);

  info("Installing package");

  // The profile contains one initialized origin directory and the storage
  // database:
  // - storage/default/https+++foo.example.com
  // - storage.sqlite
  // The file make_unsetLastAccessTime.js was run locally, specifically it was
  // temporarily enabled in xpcshell.ini and then executed:
  // mach test --interactive dom/quota/test/xpcshell/make_unsetLastAccessTime.js
  // Note: to make it become the profile in the test, additional manual steps
  // are needed.
  // 1. Remove the folder "storage/temporary".
  // 2. Remove the file "storage/ls-archive.sqlite".
  installPackage("unsetLastAccessTime_profile");

  info("Verifying last access time");

  Assert.equal(getLastAccessTime(), INT64_MIN, "Correct last access time");

  info("Initializing");

  request = init();
  await requestFinished(request);

  info("Initializing temporary storage");

  request = initTemporaryStorage();
  await requestFinished(request);

  info("Verifying last access time");

  Assert.notEqual(getLastAccessTime(), INT64_MIN, "Correct last access time");
}
