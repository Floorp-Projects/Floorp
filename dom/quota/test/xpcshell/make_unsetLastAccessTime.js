/*
  Any copyright is dedicated to the Public Domain.
  http://creativecommons.org/publicdomain/zero/1.0/
*/

async function testSteps() {
  const originDirPath = "storage/default/https+++foo.example.com";

  info("Initializing");

  let request = init();
  await requestFinished(request);

  info("Creating an empty origin directory");

  let originDir = getRelativeFile(originDirPath);
  originDir.create(Ci.nsIFile.DIRECTORY_TYPE, parseInt("0755", 8));

  info("Initializing temporary storage");

  request = initTemporaryStorage();
  await requestFinished(request);

  // The metadata file should be now restored.
}
