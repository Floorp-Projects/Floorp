/**
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

const SEC_PER_MONTH = 60 * 60 * 24 * 30;

async function testSteps() {
  function getHostname(index) {
    return "www.example" + index + ".com";
  }

  function getOrigin(index) {
    return "https://" + getHostname(index);
  }

  function getOriginDir(index) {
    return getRelativeFile("storage/default/https+++" + getHostname(index));
  }

  function updateOriginLastAccessTime(index, deltaSec) {
    let originDir = getOriginDir(index);

    let metadataFile = originDir.clone();
    metadataFile.append(".metadata-v2");

    let fileRandomAccessStream = Cc[
      "@mozilla.org/network/file-random-access-stream;1"
    ].createInstance(Ci.nsIFileRandomAccessStream);
    fileRandomAccessStream.init(metadataFile, -1, -1, 0);

    let binaryInputStream = Cc[
      "@mozilla.org/binaryinputstream;1"
    ].createInstance(Ci.nsIBinaryInputStream);
    binaryInputStream.setInputStream(fileRandomAccessStream);

    let lastAccessTime = binaryInputStream.read64();

    let seekableStream = fileRandomAccessStream.QueryInterface(
      Ci.nsISeekableStream
    );
    seekableStream.seek(Ci.nsISeekableStream.NS_SEEK_SET, 0);

    binaryOutputStream = Cc["@mozilla.org/binaryoutputstream;1"].createInstance(
      Ci.nsIBinaryOutputStream
    );
    binaryOutputStream.setOutputStream(fileRandomAccessStream);

    binaryOutputStream.write64(lastAccessTime + deltaSec * PR_USEC_PER_SEC);

    binaryOutputStream.close();

    binaryInputStream.close();
  }

  function verifyOriginDir(index, shouldExist) {
    let originDir = getOriginDir(index);
    let exists = originDir.exists();
    if (shouldExist) {
      ok(exists, "Origin directory does exist");
    } else {
      ok(!exists, "Origin directory doesn't exist");
    }
  }

  info("Setting prefs");

  Services.prefs.setBoolPref("dom.quotaManager.loadQuotaFromCache", false);
  Services.prefs.setBoolPref("dom.quotaManager.checkQuotaInfoLoadTime", true);
  Services.prefs.setIntPref(
    "dom.quotaManager.longQuotaInfoLoadTimeThresholdMs",
    0
  );

  info("Initializing");

  request = init();
  await requestFinished(request);

  info("Initializing temporary storage");

  request = initTemporaryStorage();
  await requestFinished(request);

  info("Initializing origins");

  for (let index = 0; index < 30; index++) {
    request = initTemporaryOrigin("default", getPrincipal(getOrigin(index)));
    await requestFinished(request);
  }

  info("Updating last access time of selected origins");

  for (let index = 0; index < 10; index++) {
    updateOriginLastAccessTime(index, -14 * SEC_PER_MONTH);
  }

  for (let index = 10; index < 20; index++) {
    updateOriginLastAccessTime(index, -7 * SEC_PER_MONTH);
  }

  info("Resetting");

  request = reset();
  await requestFinished(request);

  info("Setting pref");

  Services.prefs.setIntPref(
    "dom.quotaManager.unaccessedForLongTimeThresholdSec",
    13 * SEC_PER_MONTH
  );

  info("Initializing");

  request = init();
  await requestFinished(request);

  info("Initializing temporary storage");

  request = initTemporaryStorage();
  await requestFinished(request);

  info("Verifying origin directories");

  for (let index = 0; index < 10; index++) {
    verifyOriginDir(index, false);
  }
  for (let index = 10; index < 30; index++) {
    verifyOriginDir(index, true);
  }

  info("Resetting");

  request = reset();
  await requestFinished(request);

  info("Setting pref");

  Services.prefs.setIntPref(
    "dom.quotaManager.unaccessedForLongTimeThresholdSec",
    6 * SEC_PER_MONTH
  );

  info("Initializing");

  request = init();
  await requestFinished(request);

  info("Initializing temporary storage");

  request = initTemporaryStorage();
  await requestFinished(request);

  info("Verifying origin directories");

  for (let index = 0; index < 20; index++) {
    verifyOriginDir(index, false);
  }
  for (let index = 20; index < 30; index++) {
    verifyOriginDir(index, true);
  }

  info("Resetting");

  request = reset();
  await requestFinished(request);
}
