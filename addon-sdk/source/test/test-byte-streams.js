/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const byteStreams = require("sdk/io/byte-streams");
const file = require("sdk/io/file");
const { pathFor } = require("sdk/system");
const { Loader } = require("sdk/test/loader");

const STREAM_CLOSED_ERROR = new RegExp("The stream is closed and cannot be used.");

// This should match the constant of the same name in byte-streams.js.
const BUFFER_BYTE_LEN = 0x8000;

exports.testWriteRead = function (assert) {
  let fname = dataFileFilename();

  // Write a small string less than the stream's buffer size...
  let str = "exports.testWriteRead data!";
  let stream = open(assert, fname, true);
  assert.ok(!stream.closed, "stream.closed after open should be false");
  stream.write(str);
  stream.close();
  assert.ok(stream.closed, "Stream should be closed after stream.close");
  assert.throws(() => stream.write("This shouldn't be written!"),
    STREAM_CLOSED_ERROR,
    "stream.write after close should raise error");

  // ... and read it.
  stream = open(assert, fname);
  assert.equal(stream.read(), str,
    "stream.read should return string written");
  assert.equal(stream.read(), "",
    "stream.read at EOS should return empty string");
  stream.close();
  assert.ok(stream.closed, "Stream should be closed after stream.close");
  assert.throws(() => stream.read(),
    STREAM_CLOSED_ERROR,
    "stream.read after close should raise error");

  file.remove(fname);
};

// Write a big string many times the size of the stream's buffer and read it.
exports.testWriteReadBig = function (assert) {
  let str = "";
  let bufLen = BUFFER_BYTE_LEN;
  let fileSize = bufLen * 10;
  for (let i = 0; i < fileSize; i++)
    str += i % 10;
  let fname = dataFileFilename();
  let stream = open(assert, fname, true);
  stream.write(str);
  stream.close();
  stream = open(assert, fname);
  assert.equal(stream.read(), str,
    "stream.read should return string written");
  stream.close();
  file.remove(fname);
};

// The same, but write and read in chunks.
exports.testWriteReadChunks = function (assert) {
  let str = "";
  let bufLen = BUFFER_BYTE_LEN;
  let fileSize = bufLen * 10;
  for (let i = 0; i < fileSize; i++)
    str += i % 10;
  let fname = dataFileFilename();
  let stream = open(assert, fname, true);
  let i = 0;
  while (i < str.length) {
    // Use a chunk length that spans buffers.
    let chunk = str.substr(i, bufLen + 1);
    stream.write(chunk);
    i += bufLen + 1;
  }
  stream.close();
  stream = open(assert, fname);
  let readStr = "";
  bufLen = BUFFER_BYTE_LEN;
  let readLen = bufLen + 1;
  do {
    var frag = stream.read(readLen);
    readStr += frag;
  } while (frag);
  stream.close();
  assert.equal(readStr, str,
    "stream.write and read in chunks should work as expected");
  file.remove(fname);
};

exports.testReadLengths = function (assert) {
  let fname = dataFileFilename();
  let str = "exports.testReadLengths data!";
  let stream = open(assert, fname, true);
  stream.write(str);
  stream.close();

  stream = open(assert, fname);
  assert.equal(stream.read(str.length * 1000), str,
    "stream.read with big byte length should return string " +
    "written");
  stream.close();

  stream = open(assert, fname);
  assert.equal(stream.read(0), "",
    "string.read with zero byte length should return empty " +
    "string");
  stream.close();

  stream = open(assert, fname);
  assert.equal(stream.read(-1), "",
    "string.read with negative byte length should return " +
    "empty string");
  stream.close();

  file.remove(fname);
};

exports.testTruncate = function (assert) {
  let fname = dataFileFilename();
  let str = "exports.testReadLengths data!";
  let stream = open(assert, fname, true);
  stream.write(str);
  stream.close();

  stream = open(assert, fname);
  assert.equal(stream.read(), str,
    "stream.read should return string written");
  stream.close();

  stream = open(assert, fname, true);
  stream.close();

  stream = open(assert, fname);
  assert.equal(stream.read(), "",
    "stream.read after truncate should be empty");
  stream.close();

  file.remove(fname);
};

exports.testUnload = function (assert) {
  let loader = Loader(module);
  let file = loader.require("sdk/io/file");

  let filename = dataFileFilename("temp-b");
  let stream = file.open(filename, "wb");

  loader.unload();
  assert.ok(stream.closed, "Stream should be closed after module unload");
};

// Returns the name of a file that should be used to test writing and reading.
function dataFileFilename() {
  return file.join(pathFor("ProfD"), "test-byte-streams-data");
}

// Opens and returns the given file and ensures it's of the correct class.
function open(assert, filename, forWriting) {
  let stream = file.open(filename, forWriting ? "wb" : "b");
  let klass = forWriting ? "ByteWriter" : "ByteReader";
  assert.ok(stream instanceof byteStreams[klass],
           "Opened stream should be a " + klass);
  return stream;
}

require('sdk/test').run(exports);
