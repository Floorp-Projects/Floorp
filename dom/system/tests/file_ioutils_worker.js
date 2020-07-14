// Any copyright is dedicated to the Public Domain.
// - http://creativecommons.org/publicdomain/zero/1.0/

/* eslint-env mozilla/chrome-worker, node */
/* global finish, log*/

"use strict";

importScripts("chrome://mochikit/content/tests/SimpleTest/WorkerSimpleTest.js");
importScripts("resource://gre/modules/ObjectUtils.jsm");

// TODO: Remove this import for OS.File. It is currently being used as a
//       stop gap for missing IOUtils functionality.
importScripts("resource://gre/modules/osfile.jsm");

self.onmessage = async function(msg) {
  // IOUtils functionality is the same when called from the main thread, or a
  // web worker. These tests are a modified subset of the main thread tests, and
  // serve as a sanity check that the implementation is thread-safe.
  await test_api_is_available_on_worker();
  await test_full_read_and_write();

  finish();
  info("test_ioutils_worker.xhtml: Test finished");

  async function test_api_is_available_on_worker() {
    ok(self.IOUtils, "IOUtils is present in web workers");
  }

  async function test_full_read_and_write() {
    // Write a file.
    const tmpFileName = "test_ioutils_numbers.tmp";
    const bytes = Uint8Array.of(...new Array(50).keys());
    const bytesWritten = await self.IOUtils.writeAtomic(tmpFileName, bytes);
    is(
      bytesWritten,
      50,
      "IOUtils::writeAtomic can write entire byte array to file"
    );

    // Read it back.
    let fileContents = await self.IOUtils.read(tmpFileName);
    ok(
      ObjectUtils.deepEqual(bytes, fileContents) &&
        bytes.length == fileContents.length,
      "IOUtils::read can read back entire file"
    );

    const tooManyBytes = bytes.length + 1;
    fileContents = await self.IOUtils.read(tmpFileName, tooManyBytes);
    ok(
      ObjectUtils.deepEqual(bytes, fileContents) &&
        fileContents.length == bytes.length,
      "IOUtils::read can read entire file when requested maxBytes is too large"
    );

    cleanup(tmpFileName);
  }
};

function cleanup(...files) {
  files.forEach(file => {
    OS.File.remove(file);
    ok(!OS.File.exists(file), `Removed temporary file: ${file}`);
  });
}
