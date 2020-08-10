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
  const tmpDir = OS.Constants.Path.tmpDir;

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
    const tmpFileName = OS.Path.join(tmpDir, "test_ioutils_numbers.tmp");
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

    await cleanup(tmpFileName);
  }

  async function test_move_file() {
    const src = OS.Path.join(tmpDir, "test_move_file_src.tmp");
    const dest = OS.Path.join(tmpDir, "test_move_file_dest.tmp");
    const bytes = Uint8Array.of(...new Array(50).keys());
    await self.IOUtils.writeAtomic(src, bytes);

    await self.IOUtils.move(src, dest);
    ok(
      !(await fileExists(src)) && (await fileExists(dest)),
      "IOUtils::move can move files from a worker"
    );

    await cleanup(dest);
  }

  async function test_copy_file() {
    const tmpFileName = OS.Path.join(tmpDir, "test_ioutils_orig.tmp");
    const destFileName = OS.Path.join(tmpDir, "test_ioutils_copy.tmp");
    await createFile(tmpFileName, "original");

    await self.IOUtils.copy(tmpFileName, destFileName);
    ok(
      (await fileExists(tmpFileName)) &&
        (await fileHasTextContents(destFileName, "original")),
      "IOUtils::copy can copy source to dest in same directory"
    );

    await cleanup(tmpFileName, destFileName);
  }

  async function test_make_directory() {
    const dir = OS.Path.join(tmpDir, "test_make_dir.tmp.d");
    await self.IOUtils.makeDirectory(dir);
    ok(
      OS.File.stat(dir).isDir,
      "IOUtils::makeDirectory can make a new directory from a worker"
    );

    await cleanup(dir);
  }

  // Utility functions.

  Uint8Array.prototype.equals = function equals(other) {
    if (this.byteLength !== other.byteLength) {
      return false;
    }
    return this.every((val, i) => val === other[i]);
  };

  async function cleanup(...files) {
    for (const file of files) {
      await self.IOUtils.remove(file, { ignoreAbsent: true, recursive: true });
      const exists = await fileOrDirExists(file);
      ok(!exists, `Removed temporary file: ${file}`);
    }
  }

  async function createFile(location, contents = "") {
    if (typeof contents === "string") {
      contents = new TextEncoder().encode(contents);
    }
    await self.IOUtils.writeAtomic(location, contents);
    const exists = await fileExists(location);
    ok(exists, `Created temporary file at: ${location}`);
  }

  async function fileOrDirExists(location) {
    try {
      await self.IOUtils.stat(location);
      return true;
    } catch (ex) {
      return false;
    }
  }

  async function fileExists(location) {
    try {
      let { type } = await self.IOUtils.stat(location);
      return type === "regular";
    } catch (ex) {
      return false;
    }
  }

  async function fileHasTextContents(location, expectedContents) {
    if (typeof expectedContents !== "string") {
      throw new TypeError("expectedContents must be a string");
    }
    info(`Opening ${location} for reading`);
    const bytes = await self.IOUtils.read(location);
    const contents = new TextDecoder().decode(bytes);
    return contents === expectedContents;
  }
};
