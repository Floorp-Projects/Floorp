/**
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

/**
 * This test is mainly to verify that metadata files with old group information
 * get updated. See bug 1535995.
 */

loadScript("dom/quota/test/common/file.js");

async function testSteps() {
  const principal = getPrincipal("https://foo.bar.mozilla-iot.org");
  const metadataFile = getRelativeFile(
    "storage/default/https+++foo.bar.mozilla-iot.org/.metadata-v2"
  );

  async function readMetadataFile() {
    let file = await File.createFromNsIFile(metadataFile);

    let buffer = await new Promise(resolve => {
      let reader = new FileReader();
      reader.onloadend = () => resolve(reader.result);
      reader.readAsArrayBuffer(file);
    });

    return buffer;
  }

  info("Clearing");

  let request = clear();
  await requestFinished(request);

  info("Installing package");

  // The profile contains one initialized origin directory, a script for origin
  // initialization and the storage database:
  // - storage/default/https+++foo.bar.mozilla-iot.org
  // - create_db.js
  // - storage.sqlite
  // The file create_db.js in the package was run locally, specifically it was
  // temporarily added to xpcshell.ini and then executed:
  //   mach xpcshell-test --interactive dom/localstorage/test/xpcshell/create_db.js
  // Note: to make it become the profile in the test, additional manual steps
  // are needed.
  // 1. Manually change the group in .metadata and .metadata-v2 from
  //    "bar.mozilla-iot.org" to "mozilla-iot.org".
  // 2. Remove the folder "storage/temporary".
  // 3. Remove the file "storage/ls-archive.sqlite".
  installPackage("groupMismatch_profile");

  info("Reading out contents of metadata file");

  let metadataBuffer = await readMetadataFile();

  info("Initializing origin");

  request = initStorageAndOrigin(principal, "default");
  await requestFinished(request);

  info("Reading out contents of metadata file");

  let metadataBuffer2 = await readMetadataFile();

  info("Verifying blobs differ");

  ok(!compareBuffers(metadataBuffer, metadataBuffer2), "Metadata differ");
}
