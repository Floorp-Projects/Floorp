/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

const { pathFor } = require('sdk/system');
const file = require("sdk/io/file");
const url = require("sdk/url");

const byteStreams = require("sdk/io/byte-streams");
const textStreams = require("sdk/io/text-streams");

const ERRORS = {
  FILE_NOT_FOUND: /^path does not exist: .+$/,
  NOT_A_DIRECTORY: /^path is not a directory: .+$/,
  NOT_A_FILE: /^path is not a file: .+$/,
};

// Use profile directory to list / read / write files.
const profilePath = pathFor('ProfD');
const fileNameInProfile = 'compatibility.ini';
const dirNameInProfile = 'extensions';
const filePathInProfile = file.join(profilePath, fileNameInProfile);
const dirPathInProfile = file.join(profilePath, dirNameInProfile);

exports.testDirName = function(assert) {
  assert.equal(file.dirname(dirPathInProfile), profilePath,
                   "file.dirname() of dir should return parent dir");

  assert.equal(file.dirname(filePathInProfile), profilePath,
                   "file.dirname() of file should return its dir");

  let dir = profilePath;
  while (dir)
    dir = file.dirname(dir);

  assert.equal(dir, "",
                   "dirname should return empty string when dir has no parent");
};

exports.testBasename = function(assert) {
  // Get the top-most path -- the path with no basename.  E.g., on Unix-like
  // systems this will be /.  We'll use it below to build up some test paths.
  // We have to go to this trouble because file.join() needs a legal path as a
  // base case; join("foo", "bar") doesn't work unfortunately.
  let topPath = profilePath;
  let parentPath = file.dirname(topPath);
  while (parentPath) {
    topPath = parentPath;
    parentPath = file.dirname(topPath);
  }

  let path = topPath;
  assert.equal(file.basename(path), "",
                   "basename should work on paths with no components");

  path = file.join(path, "foo");
  assert.equal(file.basename(path), "foo",
                   "basename should work on paths with a single component");

  path = file.join(path, "bar");
  assert.equal(file.basename(path), "bar",
                   "basename should work on paths with multiple components");
};

exports.testList = function(assert) {
  let list = file.list(profilePath);
  let found = [ true for (name of list)
                    if (name === fileNameInProfile) ];

  if (found.length > 1)
    assert.fail("a dir can't contain two files of the same name!");
  assert.equal(found[0], true, "file.list() should work");

  assert.throws(function() {
    file.list(filePathInProfile);
  }, ERRORS.NOT_A_DIRECTORY, "file.list() on non-dir should raise error");

  assert.throws(function() {
    file.list(file.join(dirPathInProfile, "does-not-exist"));
  }, ERRORS.FILE_NOT_FOUND, "file.list() on nonexistent should raise error");
};

exports.testRead = function(assert) {
  let contents = file.read(filePathInProfile);
  assert.ok(/Compatibility/.test(contents),
                     "file.read() should work");

  assert.throws(function() {
    file.read(file.join(dirPathInProfile, "does-not-exists"));
  }, ERRORS.FILE_NOT_FOUND, "file.read() on nonexistent file should throw");

  assert.throws(function() {
    file.read(dirPathInProfile);
  }, ERRORS.NOT_A_FILE, "file.read() on dir should throw");
};

exports.testJoin = function(assert) {
  let baseDir = file.dirname(filePathInProfile);

  assert.equal(file.join(baseDir, fileNameInProfile),
                   filePathInProfile, "file.join() should work");
};

exports.testOpenNonexistentForRead = function (assert) {
  var filename = file.join(profilePath, 'does-not-exists');
  assert.throws(function() {
    file.open(filename);
  }, ERRORS.FILE_NOT_FOUND, "file.open() on nonexistent file should throw");

  assert.throws(function() {
    file.open(filename, "r");
  }, ERRORS.FILE_NOT_FOUND, "file.open('r') on nonexistent file should throw");

  assert.throws(function() {
    file.open(filename, "zz");
  }, ERRORS.FILE_NOT_FOUND, "file.open('zz') on nonexistent file should throw");
};

exports.testOpenNonexistentForWrite = function (assert) {
  let filename = file.join(profilePath, 'open.txt');

  let stream = file.open(filename, "w");
  stream.close();

  assert.ok(file.exists(filename),
              "file.exists() should return true after file.open('w')");
  file.remove(filename);
  assert.ok(!file.exists(filename),
              "file.exists() should return false after file.remove()");

  stream = file.open(filename, "rw");
  stream.close();

  assert.ok(file.exists(filename),
              "file.exists() should return true after file.open('rw')");
  file.remove(filename);
  assert.ok(!file.exists(filename),
              "file.exists() should return false after file.remove()");
};

exports.testOpenDirectory = function (assert) {
  let dir = dirPathInProfile;
  assert.throws(function() {
    file.open(dir);
  }, ERRORS.NOT_A_FILE, "file.open() on directory should throw");

  assert.throws(function() {
    file.open(dir, "w");
  }, ERRORS.NOT_A_FILE, "file.open('w') on directory should throw");
};

exports.testOpenTypes = function (assert) {
  let filename = file.join(profilePath, 'open-types.txt');


  // Do the opens first to create the data file.
  var stream = file.open(filename, "w");
  assert.ok(stream instanceof textStreams.TextWriter,
              "open(w) should return a TextWriter");
  stream.close();

  stream = file.open(filename, "wb");
  assert.ok(stream instanceof byteStreams.ByteWriter,
              "open(wb) should return a ByteWriter");
  stream.close();

  stream = file.open(filename);
  assert.ok(stream instanceof textStreams.TextReader,
              "open() should return a TextReader");
  stream.close();

  stream = file.open(filename, "r");
  assert.ok(stream instanceof textStreams.TextReader,
              "open(r) should return a TextReader");
  stream.close();

  stream = file.open(filename, "b");
  assert.ok(stream instanceof byteStreams.ByteReader,
              "open(b) should return a ByteReader");
  stream.close();

  stream = file.open(filename, "rb");
  assert.ok(stream instanceof byteStreams.ByteReader,
              "open(rb) should return a ByteReader");
  stream.close();

  file.remove(filename);
};

exports.testMkpathRmdir = function (assert) {
  let basePath = profilePath;
  let dirs = [];
  for (let i = 0; i < 3; i++)
    dirs.push("test-file-dir");

  let paths = [];
  for (let i = 0; i < dirs.length; i++) {
    let args = [basePath].concat(dirs.slice(0, i + 1));
    paths.unshift(file.join.apply(null, args));
  }

  for (let i = 0; i < paths.length; i++) {
    assert.ok(!file.exists(paths[i]),
                "Sanity check: path should not exist: " + paths[i]);
  }

  file.mkpath(paths[0]);
  assert.ok(file.exists(paths[0]), "mkpath should create path: " + paths[0]);

  for (let i = 0; i < paths.length; i++) {
    file.rmdir(paths[i]);
    assert.ok(!file.exists(paths[i]),
                "rmdir should remove path: " + paths[i]);
  }
};

exports.testMkpathTwice = function (assert) {
  let dir = profilePath;
  let path = file.join(dir, "test-file-dir");
  assert.ok(!file.exists(path),
              "Sanity check: path should not exist: " + path);
  file.mkpath(path);
  assert.ok(file.exists(path), "mkpath should create path: " + path);
  file.mkpath(path);
  assert.ok(file.exists(path),
              "After second mkpath, path should still exist: " + path);
  file.rmdir(path);
  assert.ok(!file.exists(path), "rmdir should remove path: " + path);
};

exports.testMkpathExistingNondirectory = function (assert) {
  var fname = file.join(profilePath, 'conflict.txt');
  file.open(fname, "w").close();
  assert.ok(file.exists(fname), "File should exist");
  assert.throws(function() file.mkpath(fname),
                    /^The path already exists and is not a directory: .+$/,
                    "mkpath on file should raise error");
  file.remove(fname);
};

exports.testRmdirNondirectory = function (assert) {
  var fname = file.join(profilePath, 'not-a-dir')
  file.open(fname, "w").close();
  assert.ok(file.exists(fname), "File should exist");
  assert.throws(function() {
    file.rmdir(fname);
  }, ERRORS.NOT_A_DIRECTORY, "rmdir on file should raise error");
  file.remove(fname);
  assert.ok(!file.exists(fname), "File should not exist");
  assert.throws(function () file.rmdir(fname),
                    ERRORS.FILE_NOT_FOUND,
                    "rmdir on non-existing file should raise error");
};

exports.testRmdirNonempty = function (assert) {
  let dir = profilePath;
  let path = file.join(dir, "test-file-dir");
  assert.ok(!file.exists(path),
              "Sanity check: path should not exist: " + path);
  file.mkpath(path);
  let filePath = file.join(path, "file");
  file.open(filePath, "w").close();
  assert.ok(file.exists(filePath),
              "Sanity check: path should exist: " + filePath);
  assert.throws(function () file.rmdir(path),
                    /^The directory is not empty: .+$/,
                    "rmdir on non-empty directory should raise error");
  file.remove(filePath);
  file.rmdir(path);
  assert.ok(!file.exists(path), "Path should not exist");
};

require('sdk/test').run(exports);
