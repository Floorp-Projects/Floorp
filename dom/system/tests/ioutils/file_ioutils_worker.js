// Any copyright is dedicated to the Public Domain.
// - http://creativecommons.org/publicdomain/zero/1.0/

// Portions of this file are originally from narwhal.js (http://narwhaljs.org)
// Copyright (c) 2009 Thomas Robinson <280north.com>
// MIT license: http://opensource.org/licenses/MIT

/* eslint-env worker */

"use strict";

/* import-globals-from /testing/mochitest/tests/SimpleTest/WorkerSimpleTest.js */
importScripts("chrome://mochikit/content/tests/SimpleTest/WorkerSimpleTest.js");

importScripts("file_ioutils_test_fixtures.js");

self.onmessage = async function (msg) {
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
      _deepEqual(bytes, fileContents) && bytes.length == fileContents.length,
      "IOUtils::read can read back entire file"
    );

    const tooManyBytes = bytes.length + 1;
    fileContents = await IOUtils.read(tmpFileName, { maxBytes: tooManyBytes });
    ok(
      _deepEqual(bytes, fileContents) && fileContents.length == bytes.length,
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

// This is copied from the ObjectUtils module, as it is difficult to translate
// file_ioutils_test_fixtures.js into a ES module and have it used in non-module
// contexts.

// ... Start of previously MIT-licensed code.
// This deepEqual implementation is originally from narwhal.js (http://narwhaljs.org)
// Copyright (c) 2009 Thomas Robinson <280north.com>
// MIT license: http://opensource.org/licenses/MIT

function _deepEqual(a, b) {
  // The numbering below refers to sections in the CommonJS spec.

  // 7.1 All identical values are equivalent, as determined by ===.
  if (a === b) {
    return true;
    // 7.2 If the b value is a Date object, the a value is
    // equivalent if it is also a Date object that refers to the same time.
  }
  let aIsDate = instanceOf(a, "Date");
  let bIsDate = instanceOf(b, "Date");
  if (aIsDate || bIsDate) {
    if (!aIsDate || !bIsDate) {
      return false;
    }
    if (isNaN(a.getTime()) && isNaN(b.getTime())) {
      return true;
    }
    return a.getTime() === b.getTime();
    // 7.3 If the b value is a RegExp object, the a value is
    // equivalent if it is also a RegExp object with the same source and
    // properties (`global`, `multiline`, `lastIndex`, `ignoreCase`).
  }
  let aIsRegExp = instanceOf(a, "RegExp");
  let bIsRegExp = instanceOf(b, "RegExp");
  if (aIsRegExp || bIsRegExp) {
    return (
      aIsRegExp &&
      bIsRegExp &&
      a.source === b.source &&
      a.global === b.global &&
      a.multiline === b.multiline &&
      a.lastIndex === b.lastIndex &&
      a.ignoreCase === b.ignoreCase
    );
    // 7.4 Other pairs that do not both pass typeof value == "object",
    // equivalence is determined by ==.
  }
  if (typeof a != "object" || typeof b != "object") {
    return a == b;
  }
  // 7.5 For all other Object pairs, including Array objects, equivalence is
  // determined by having the same number of owned properties (as verified
  // with Object.prototype.hasOwnProperty.call), the same set of keys
  // (although not necessarily the same order), equivalent values for every
  // corresponding key, and an identical 'prototype' property. Note: this
  // accounts for both named and indexed properties on Arrays.
  return objEquiv(a, b);
}

function instanceOf(object, type) {
  return Object.prototype.toString.call(object) == "[object " + type + "]";
}

function isUndefinedOrNull(value) {
  return value === null || value === undefined;
}

function isArguments(object) {
  return instanceOf(object, "Arguments");
}

function objEquiv(a, b) {
  if (isUndefinedOrNull(a) || isUndefinedOrNull(b)) {
    return false;
  }
  // An identical 'prototype' property.
  if ((a.prototype || undefined) != (b.prototype || undefined)) {
    return false;
  }
  // Object.keys may be broken through screwy arguments passing. Converting to
  // an array solves the problem.
  if (isArguments(a)) {
    if (!isArguments(b)) {
      return false;
    }
    a = Array.prototype.slice.call(a);
    b = Array.prototype.slice.call(b);
    return _deepEqual(a, b);
  }
  let ka, kb;
  try {
    ka = Object.keys(a);
    kb = Object.keys(b);
  } catch (e) {
    // Happens when one is a string literal and the other isn't
    return false;
  }
  // Having the same number of owned properties (keys incorporates
  // hasOwnProperty)
  if (ka.length != kb.length) {
    return false;
  }
  // The same set of keys (although not necessarily the same order),
  ka.sort();
  kb.sort();
  // Equivalent values for every corresponding key, and possibly expensive deep
  // test
  for (let key of ka) {
    if (!_deepEqual(a[key], b[key])) {
      return false;
    }
  }
  return true;
}

// ... End of previously MIT-licensed code.
