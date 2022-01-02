/**
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

/* import-globals-from ../helpers.js */

var disableWorkerTest = "FileHandle doesn't work in workers yet";

async function testSteps() {
  const name = "test_filehandle_read_beyond_eof_exception.js";

  info("Opening database");
  let dbRequest = indexedDB.open(name);
  await expectingUpgrade(dbRequest);
  let event = await expectingSuccess(dbRequest);

  info("Creating file");
  const file = event.target.result.createMutableFile("F");
  event = await expectingSuccess(file);

  info("Opening and reading from empty file");
  const handle = event.target.result.open("readonly");
  const fileRequest = handle.readAsArrayBuffer(8);
  await expectingError(fileRequest, "UnknownError");
}
