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

exports.testDirName = function(test) {
  test.assertEqual(file.dirname(dirPathInProfile), profilePath,
                   "file.dirname() of dir should return parent dir");

  test.assertEqual(file.dirname(filePathInProfile), profilePath,
                   "file.dirname() of file should return its dir");

  let dir = profilePath;
  while (dir)
    dir = file.dirname(dir);

  test.assertEqual(dir, "",
                   "dirname should return empty string when dir has no parent");
};

exports.testBasename = function(test) {
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
  test.assertEqual(file.basename(path), "",
                   "basename should work on paths with no components");

  path = file.join(path, "foo");
  test.assertEqual(file.basename(path), "foo",
                   "basename should work on paths with a single component");

  path = file.join(path, "bar");
  test.assertEqual(file.basename(path), "bar",
                   "basename should work on paths with multiple components");
};

exports.testList = function(test) {
  let list = file.list(profilePath);
  let found = [ true for each (name in list)
                    if (name === fileNameInProfile) ];

  if (found.length > 1)
    test.fail("a dir can't contain two files of the same name!");
  test.assertEqual(found[0], true, "file.list() should work");

  test.assertRaises(function() {
    file.list(filePathInProfile);
  }, ERRORS.NOT_A_DIRECTORY, "file.list() on non-dir should raise error");

  test.assertRaises(function() {
    file.list(file.join(dirPathInProfile, "does-not-exist"));
  }, ERRORS.FILE_NOT_FOUND, "file.list() on nonexistent should raise error");
};

exports.testRead = function(test) {
  let contents = file.read(filePathInProfile);
  test.assertMatches(contents, /Compatibility/,
                     "file.read() should work");

  test.assertRaises(function() {
    file.read(file.join(dirPathInProfile, "does-not-exists"));
  }, ERRORS.FILE_NOT_FOUND, "file.read() on nonexistent file should throw");

  test.assertRaises(function() {
    file.read(dirPathInProfile);
  }, ERRORS.NOT_A_FILE, "file.read() on dir should throw");
};

exports.testJoin = function(test) {
  let baseDir = file.dirname(filePathInProfile);

  test.assertEqual(file.join(baseDir, fileNameInProfile),
                   filePathInProfile, "file.join() should work");
};

exports.testOpenNonexistentForRead = function (test) {
  var filename = file.join(profilePath, 'does-not-exists');
  test.assertRaises(function() {
    file.open(filename);
  }, ERRORS.FILE_NOT_FOUND, "file.open() on nonexistent file should throw");

  test.assertRaises(function() {
    file.open(filename, "r");
  }, ERRORS.FILE_NOT_FOUND, "file.open('r') on nonexistent file should throw");

  test.assertRaises(function() {
    file.open(filename, "zz");
  }, ERRORS.FILE_NOT_FOUND, "file.open('zz') on nonexistent file should throw");
};

exports.testOpenNonexistentForWrite = function (test) {
  let filename = file.join(profilePath, 'open.txt');

  let stream = file.open(filename, "w");
  stream.close();

  test.assert(file.exists(filename),
              "file.exists() should return true after file.open('w')");
  file.remove(filename);
  test.assert(!file.exists(filename),
              "file.exists() should return false after file.remove()");

  stream = file.open(filename, "rw");
  stream.close();

  test.assert(file.exists(filename),
              "file.exists() should return true after file.open('rw')");
  file.remove(filename);
  test.assert(!file.exists(filename),
              "file.exists() should return false after file.remove()");
};

exports.testOpenDirectory = function (test) {
  let dir = dirPathInProfile;
  test.assertRaises(function() {
    file.open(dir);
  }, ERRORS.NOT_A_FILE, "file.open() on directory should throw");

  test.assertRaises(function() {
    file.open(dir, "w");
  }, ERRORS.NOT_A_FILE, "file.open('w') on directory should throw");
};

exports.testOpenTypes = function (test) {
  let filename = file.join(profilePath, 'open-types.txt');


  // Do the opens first to create the data file.
  var stream = file.open(filename, "w");
  test.assert(stream instanceof textStreams.TextWriter,
              "open(w) should return a TextWriter");
  stream.close();

  stream = file.open(filename, "wb");
  test.assert(stream instanceof byteStreams.ByteWriter,
              "open(wb) should return a ByteWriter");
  stream.close();

  stream = file.open(filename);
  test.assert(stream instanceof textStreams.TextReader,
              "open() should return a TextReader");
  stream.close();

  stream = file.open(filename, "r");
  test.assert(stream instanceof textStreams.TextReader,
              "open(r) should return a TextReader");
  stream.close();

  stream = file.open(filename, "b");
  test.assert(stream instanceof byteStreams.ByteReader,
              "open(b) should return a ByteReader");
  stream.close();

  stream = file.open(filename, "rb");
  test.assert(stream instanceof byteStreams.ByteReader,
              "open(rb) should return a ByteReader");
  stream.close();

  file.remove(filename);
};

exports.testMkpathRmdir = function (test) {
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
    test.assert(!file.exists(paths[i]),
                "Sanity check: path should not exist: " + paths[i]);
  }

  file.mkpath(paths[0]);
  test.assert(file.exists(paths[0]), "mkpath should create path: " + paths[0]);

  for (let i = 0; i < paths.length; i++) {
    file.rmdir(paths[i]);
    test.assert(!file.exists(paths[i]),
                "rmdir should remove path: " + paths[i]);
  }
};

exports.testMkpathTwice = function (test) {
  let dir = profilePath;
  let path = file.join(dir, "test-file-dir");
  test.assert(!file.exists(path),
              "Sanity check: path should not exist: " + path);
  file.mkpath(path);
  test.assert(file.exists(path), "mkpath should create path: " + path);
  file.mkpath(path);
  test.assert(file.exists(path),
              "After second mkpath, path should still exist: " + path);
  file.rmdir(path);
  test.assert(!file.exists(path), "rmdir should remove path: " + path);
};

exports.testMkpathExistingNondirectory = function (test) {
  var fname = file.join(profilePath, 'conflict.txt');
  file.open(fname, "w").close();
  test.assert(file.exists(fname), "File should exist");
  test.assertRaises(function() file.mkpath(fname),
                    /^The path already exists and is not a directory: .+$/,
                    "mkpath on file should raise error");
  file.remove(fname);
};

exports.testRmdirNondirectory = function (test) {
  var fname = file.join(profilePath, 'not-a-dir')
  file.open(fname, "w").close();
  test.assert(file.exists(fname), "File should exist");
  test.assertRaises(function() {
    file.rmdir(fname);
  }, ERRORS.NOT_A_DIRECTORY, "rmdir on file should raise error");
  file.remove(fname);
  test.assert(!file.exists(fname), "File should not exist");
  test.assertRaises(function () file.rmdir(fname),
                    ERRORS.FILE_NOT_FOUND,
                    "rmdir on non-existing file should raise error");
};

exports.testRmdirNonempty = function (test) {
  let dir = profilePath;
  let path = file.join(dir, "test-file-dir");
  test.assert(!file.exists(path),
              "Sanity check: path should not exist: " + path);
  file.mkpath(path);
  let filePath = file.join(path, "file");
  file.open(filePath, "w").close();
  test.assert(file.exists(filePath),
              "Sanity check: path should exist: " + filePath);
  test.assertRaises(function () file.rmdir(path),
                    /^The directory is not empty: .+$/,
                    "rmdir on non-empty directory should raise error");
  file.remove(filePath);
  file.rmdir(path);
  test.assert(!file.exists(path), "Path should not exist");
};
