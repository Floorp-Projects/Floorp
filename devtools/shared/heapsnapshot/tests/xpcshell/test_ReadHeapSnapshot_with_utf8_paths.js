/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

// Test that we can read core dumps with a UTF8 path into HeapSnapshot instances.
/* eslint-disable strict */
const { OS } = ChromeUtils.import("resource://gre/modules/osfile.jsm");
add_task(async function() {
  const fileNameWithRussianCharacters =
    "Снимок памяти Click.ru 08.06.2020 (Firefox dump).fxsnapshot";
  const filePathWithRussianCharacters = OS.Path.join(
    OS.Constants.Path.tmpDir,
    fileNameWithRussianCharacters
  );

  const filePath = ChromeUtils.saveHeapSnapshot({ globals: [this] });
  ok(true, "Should be able to save a snapshot.");

  await OS.File.copy(filePath, filePathWithRussianCharacters);

  ok(
    await OS.File.exists(filePathWithRussianCharacters),
    `We could copy the file to the expected path ${filePathWithRussianCharacters}`
  );

  const snapshot = ChromeUtils.readHeapSnapshot(filePathWithRussianCharacters);
  ok(snapshot, "Should be able to read a heap snapshot from an utf8 path");
  ok(HeapSnapshot.isInstance(snapshot), "Should be an instanceof HeapSnapshot");
});
