// Any copyright is dedicated to the Public Domain.
// - http://creativecommons.org/publicdomain/zero/1.0/

/* eslint-env mozilla/chrome-worker, node */
/* global finish, log */

"use strict";

importScripts("chrome://mochikit/content/tests/SimpleTest/WorkerSimpleTest.js");
importScripts("resource://gre/modules/ObjectUtils.jsm");

importScripts("file_ioutils_test_fixtures.js");

self.onmessage = async function(msg) {
  const tmpDir = await PathUtils.getTempDir();

  // IOUtils functionality is the same when called from the main thread, or a
  // web worker. These tests are a modified subset of the main thread tests, and
  // serve as a confidence check that the implementation is thread-safe.
  await test_api_is_available_on_worker();
  await test_full_read_and_write();
  await test_move_file();
  await test_copy_file();
  await test_make_directory();

  finish();
  info("test_ioutils_worker.xhtml: Test finished");

  async function test_api_is_available_on_worker() {
    ok(self.IOUtils, "IOUtils is present in web workers");
  }

  async function test_full_read_and_write() {
    // Write a file.
    const tmpFileName = PathUtils.join(tmpDir, "test_ioutils_numbers.tmp");
    const bytes = Uint8Array.of(...new Array(50).keys());
    const bytesWritten = await IOUtils.write(tmpFileName, bytes);
    is(bytesWritten, 50, "IOUtils::write can write entire byte array to file");

    // Read it back.
    let fileContents = await IOUtils.read(tmpFileName);
    ok(
      ObjectUtils.deepEqual(bytes, fileContents) &&
        bytes.length == fileContents.length,
      "IOUtils::read can read back entire file"
    );

    const tooManyBytes = bytes.length + 1;
    fileContents = await IOUtils.read(tmpFileName, { maxBytes: tooManyBytes });
    ok(
      ObjectUtils.deepEqual(bytes, fileContents) &&
        fileContents.length == bytes.length,
      "IOUtils::read can read entire file when requested maxBytes is too large"
    );

    await cleanup(tmpFileName);
  }

  async function test_move_file() {
    const src = PathUtils.join(tmpDir, "test_move_file_src.tmp");
    const dest = PathUtils.join(tmpDir, "test_move_file_dest.tmp");
    const bytes = Uint8Array.of(...new Array(50).keys());
    await IOUtils.write(src, bytes);

    await IOUtils.move(src, dest);
    ok(
      !(await fileExists(src)) && (await fileExists(dest)),
      "IOUtils::move can move files from a worker"
    );

    await cleanup(dest);
  }

  async function test_copy_file() {
    const tmpFileName = PathUtils.join(tmpDir, "test_ioutils_orig.tmp");
    const destFileName = PathUtils.join(tmpDir, "test_ioutils_copy.tmp");
    await createFile(tmpFileName, "original");

    await IOUtils.copy(tmpFileName, destFileName);
    ok(
      (await fileExists(tmpFileName)) &&
        (await fileHasTextContents(destFileName, "original")),
      "IOUtils::copy can copy source to dest in same directory"
    );

    await cleanup(tmpFileName, destFileName);
  }

  async function test_make_directory() {
    const dir = PathUtils.join(tmpDir, "test_make_dir.tmp.d");
    await IOUtils.makeDirectory(dir);
    const stat = await IOUtils.stat(dir);
    is(
      stat.type,
      "directory",
      "IOUtils::makeDirectory can make a new directory from a worker"
    );

    await cleanup(dir);
  }
};
