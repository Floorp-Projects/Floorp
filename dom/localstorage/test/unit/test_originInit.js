/**
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

async function testSteps() {
  const principal = getPrincipal("http://example.com");

  const dataFile = getRelativeFile(
    "storage/default/http+++example.com/ls/data.sqlite"
  );

  const usageJournalFile = getRelativeFile(
    "storage/default/http+++example.com/ls/usage-journal"
  );

  const usageFile = getRelativeFile(
    "storage/default/http+++example.com/ls/usage"
  );

  const data = {};
  data.key = "key1";
  data.value = "value1";
  data.usage = data.key.length + data.value.length;

  const usageFileCookie = 0x420a420a;

  async function createTestOrigin() {
    let storage = getLocalStorage(principal);

    storage.setItem(data.key, data.value);

    let request = reset();
    await requestFinished(request);
  }

  function removeFile(file) {
    file.remove(false);
  }

  function createEmptyFile(file) {
    file.create(Ci.nsIFile.NORMAL_FILE_TYPE, parseInt("0644", 8));
  }

  function getBinaryOutputStream(file) {
    var ostream = Cc[
      "@mozilla.org/network/file-output-stream;1"
    ].createInstance(Ci.nsIFileOutputStream);
    ostream.init(file, -1, parseInt("0644", 8), 0);

    let bstream = Cc["@mozilla.org/binaryoutputstream;1"].createInstance(
      Ci.nsIBinaryOutputStream
    );
    bstream.setOutputStream(ostream);

    return bstream;
  }

  async function initTestOrigin() {
    let request = initOrigin(principal, "default");
    await requestFinished(request);
  }

  async function checkFiles(wantData, wantUsage) {
    let exists = dataFile.exists();
    if (wantData) {
      ok(exists, "Data file does exist");
    } else {
      ok(!exists, "Data file doesn't exist");
    }

    exists = usageJournalFile.exists();
    ok(!exists, "Usage journal file doesn't exist");

    exists = usageFile.exists();
    if (wantUsage) {
      ok(exists, "Usage file does exist");
    } else {
      ok(!exists, "Usage file doesn't exist");
      return;
    }

    let usage = await readUsageFromUsageFile(usageFile);
    ok(usage == data.usage, "Correct usage");
  }

  async function clearTestOrigin() {
    let request = clearOrigin(principal, "default");
    await requestFinished(request);
  }

  info("Setting prefs");

  Services.prefs.setBoolPref("dom.storage.next_gen", true);

  info(
    "Stage 1 - " +
      "data.sqlite file doesn't exist, " +
      "usage-journal file doesn't exist, " +
      "any usage file exists"
  );

  await createTestOrigin();

  removeFile(dataFile);

  await initTestOrigin();

  await checkFiles(/* wantData */ false, /* wantUsage */ false);

  await clearTestOrigin();

  info(
    "Stage 2 - " +
      "data.sqlite file doesn't exist, " +
      "any usage-journal file exists, " +
      "any usage file exists"
  );

  await createTestOrigin();

  removeFile(dataFile);
  createEmptyFile(usageJournalFile);

  await initTestOrigin();

  await checkFiles(/* wantData */ false, /* wantUsage */ false);

  await clearTestOrigin();

  info(
    "Stage 3 - " +
      "valid data.sqlite file exists, " +
      "usage-journal file doesn't exist, " +
      "usage file doesn't exist"
  );

  await createTestOrigin();

  removeFile(usageFile);

  await initTestOrigin();

  await checkFiles(/* wantData */ true, /* wantUsage */ true);

  await clearTestOrigin();

  info(
    "Stage 4 - " +
      "valid data.sqlite file exists, " +
      "usage-journal file doesn't exist, " +
      "invalid (wrong file size) usage file exists"
  );

  await createTestOrigin();

  removeFile(usageFile);
  createEmptyFile(usageFile);

  await initTestOrigin();

  await checkFiles(/* wantData */ true, /* wantUsage */ true);

  await clearTestOrigin();

  info(
    "Stage 5 - " +
      "valid data.sqlite file exists, " +
      "usage-journal file doesn't exist, " +
      "invalid (wrong cookie) usage file exists"
  );

  await createTestOrigin();

  let stream = getBinaryOutputStream(usageFile);
  stream.write32(usageFileCookie - 1);
  stream.write64(data.usage);
  stream.close();

  await initTestOrigin();

  await checkFiles(/* wantData */ true, /* wantUsage */ true);

  await clearTestOrigin();

  info(
    "Stage 6 - " +
      "valid data.sqlite file exists, " +
      "usage-journal file doesn't exist, " +
      "valid usage file exists"
  );

  await createTestOrigin();

  await initTestOrigin();

  await checkFiles(/* wantData */ true, /* wantUsage */ true);

  await clearTestOrigin();

  info(
    "Stage 7 - " +
      "valid data.sqlite file exists, " +
      "any usage-journal exists, " +
      "usage file doesn't exist"
  );

  await createTestOrigin();

  createEmptyFile(usageJournalFile);
  removeFile(usageFile);

  await initTestOrigin();

  await checkFiles(/* wantData */ true, /* wantUsage */ true);

  await clearTestOrigin();

  info(
    "Stage 8 - " +
      "valid data.sqlite file exists, " +
      "any usage-journal exists, " +
      "invalid (wrong file size) usage file exists"
  );

  await createTestOrigin();

  createEmptyFile(usageJournalFile);
  removeFile(usageFile);
  createEmptyFile(usageFile);

  await initTestOrigin();

  await checkFiles(/* wantData */ true, /* wantUsage */ true);

  await clearTestOrigin();

  info(
    "Stage 9 - " +
      "valid data.sqlite file exists, " +
      "any usage-journal exists, " +
      "invalid (wrong cookie) usage file exists"
  );

  await createTestOrigin();

  createEmptyFile(usageJournalFile);
  stream = getBinaryOutputStream(usageFile);
  stream.write32(usageFileCookie - 1);
  stream.write64(data.usage);
  stream.close();

  await initTestOrigin();

  await checkFiles(/* wantData */ true, /* wantUsage */ true);

  await clearTestOrigin();

  info(
    "Stage 10 - " +
      "valid data.sqlite file exists, " +
      "any usage-journal exists, " +
      "invalid (wrong usage) usage file exists"
  );

  await createTestOrigin();

  createEmptyFile(usageJournalFile);
  stream = getBinaryOutputStream(usageFile);
  stream.write32(usageFileCookie);
  stream.write64(data.usage - 1);
  stream.close();

  await initTestOrigin();

  await checkFiles(/* wantData */ true, /* wantUsage */ true);

  await clearTestOrigin();

  info(
    "Stage 11 - " +
      "valid data.sqlite file exists, " +
      "any usage-journal exists, " +
      "valid usage file exists"
  );

  await createTestOrigin();

  createEmptyFile(usageJournalFile);

  await initTestOrigin();

  await checkFiles(/* wantData */ true, /* wantUsage */ true);

  await clearTestOrigin();
}
