/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const file = require("sdk/io/file");
const { pathFor } = require("sdk/system");
const { Loader } = require("sdk/test/loader");

const STREAM_CLOSED_ERROR = new RegExp("The stream is closed and cannot be used.");

// This should match the constant of the same name in text-streams.js.
const BUFFER_BYTE_LEN = 0x8000;

exports.testWriteRead = function (assert) {
  let fname = dataFileFilename();

  // Write a small string less than the stream's buffer size...
  let str = "exports.testWriteRead data!";
  let stream = file.open(fname, "w");
  assert.ok(!stream.closed, "stream.closed after open should be false");
  stream.write(str);
  stream.close();
  assert.ok(stream.closed, "stream.closed after close should be true");
  assert.throws(function () stream.close(),
                    STREAM_CLOSED_ERROR,
                    "stream.close after already closed should raise error");
  assert.throws(function () stream.write("This shouldn't be written!"),
                    STREAM_CLOSED_ERROR,
                    "stream.write after close should raise error");

  // ... and read it.
  stream = file.open(fname);
  assert.ok(!stream.closed, "stream.closed after open should be false");
  assert.equal(stream.read(), str,
                   "stream.read should return string written");
  assert.equal(stream.read(), "",
                   "stream.read at EOS should return empty string");
  stream.close();
  assert.ok(stream.closed, "stream.closed after close should be true");
  assert.throws(function () stream.close(),
                    STREAM_CLOSED_ERROR,
                    "stream.close after already closed should raise error");
  assert.throws(function () stream.read(),
                    STREAM_CLOSED_ERROR,
                    "stream.read after close should raise error");

  // Write a big string many times the size of the stream's buffer and read it.
  // Since it comes after the previous test, this also ensures that the file is
  // truncated when it's opened for writing.
  str = "";
  let bufLen = BUFFER_BYTE_LEN;
  let fileSize = bufLen * 10;
  for (let i = 0; i < fileSize; i++)
    str += i % 10;
  stream = file.open(fname, "w");
  stream.write(str);
  stream.close();
  stream = file.open(fname);
  assert.equal(stream.read(), str,
                   "stream.read should return string written");
  stream.close();

  // The same, but write and read in chunks.
  stream = file.open(fname, "w");
  let i = 0;
  while (i < str.length) {
    // Use a chunk length that spans buffers.
    let chunk = str.substr(i, bufLen + 1);
    stream.write(chunk);
    i += bufLen + 1;
  }
  stream.close();
  stream = file.open(fname);
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

  // Read the same file, passing in strange numbers of bytes to read.
  stream = file.open(fname);
  assert.equal(stream.read(fileSize * 100), str,
                   "stream.read with big byte length should return string " +
                   "written");
  stream.close();

  stream = file.open(fname);
  assert.equal(stream.read(0), "",
                   "string.read with zero byte length should return empty " +
                   "string");
  stream.close();

  stream = file.open(fname);
  assert.equal(stream.read(-1), "",
                   "string.read with negative byte length should return " +
                   "empty string");
  stream.close();

  file.remove(fname);
};

exports.testWriteAsync = function (assert, done) {
  let fname = dataFileFilename();
  let str = "exports.testWriteAsync data!";
  let stream = file.open(fname, "w");
  assert.ok(!stream.closed, "stream.closed after open should be false");

  // Write.
  stream.writeAsync(str, function (err) {
    assert.equal(this, stream, "|this| should be the stream object");
    assert.equal(err, undefined,
                     "stream.writeAsync should not cause error");
    assert.ok(stream.closed, "stream.closed after write should be true");
    assert.throws(function () stream.close(),
                      STREAM_CLOSED_ERROR,
                      "stream.close after already closed should raise error");
    assert.throws(function () stream.writeAsync("This shouldn't work!"),
                      STREAM_CLOSED_ERROR,
                      "stream.writeAsync after close should raise error");

    // Read.
    stream = file.open(fname, "r");
    assert.ok(!stream.closed, "stream.closed after open should be false");
    let readStr = stream.read();
    assert.equal(readStr, str,
                     "string.read should yield string written");
    stream.close();
    file.remove(fname);
    done();
  });
};

exports.testUnload = function (assert) {
  let loader = Loader(module);
  let file = loader.require("sdk/io/file");

  let filename = dataFileFilename("temp");
  let stream = file.open(filename, "w");

  loader.unload();
  assert.ok(stream.closed, "stream should be closed after module unload");
};

// Returns the name of a file that should be used to test writing and reading.
function dataFileFilename() {
  return file.join(pathFor("ProfD"), "test-text-streams-data");
}

require('sdk/test').run(exports);
