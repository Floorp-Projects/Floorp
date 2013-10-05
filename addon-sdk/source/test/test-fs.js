/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { pathFor, platform } = require("sdk/system");
const fs = require("sdk/io/fs");
const url = require("sdk/url");
const path = require("sdk/fs/path");
const { defer } = require("sdk/core/promise");
const { Buffer } = require("sdk/io/buffer");
const { is } = require("sdk/system/xul-app");

// Use profile directory to list / read / write files.
const profilePath = pathFor("ProfD");
const fileNameInProfile = "compatibility.ini";
const dirNameInProfile = "extensions";
const filePathInProfile = path.join(profilePath, fileNameInProfile);
const dirPathInProfile = path.join(profilePath, dirNameInProfile);
const mkdirPath = path.join(profilePath, "sdk-fixture-mkdir");
const writePath = path.join(profilePath, "sdk-fixture-writeFile");
const unlinkPath = path.join(profilePath, "sdk-fixture-unlink");
const truncatePath = path.join(profilePath, "sdk-fixture-truncate");
const renameFromPath = path.join(profilePath, "sdk-fixture-rename-from");
const renameToPath = path.join(profilePath, "sdk-fixture-rename-to");
const chmodPath = path.join(profilePath, "sdk-fixture-chmod");

const profileEntries = [
  "compatibility.ini",
  "extensions",
  "prefs.js"
  // There are likely to be a lot more files but we can"t really
  // on consistent list so we limit to this.
];

const isWindows = platform.indexOf('win') === 0;

exports["test readdir"] = function(assert, end) {
  var async = false;
  fs.readdir(profilePath, function(error, entries) {
    assert.ok(async, "readdir is async");
    assert.ok(!error, "there is no error when reading directory");
    assert.ok(profileEntries.length <= entries.length,
              "got at least number of entries we expect");
    assert.ok(profileEntries.every(function(entry) {
                return entries.indexOf(entry) >= 0;
              }), "all profiles are present");
    end();
  });

  async = true;
};

exports["test readdir error"] = function(assert, end) {
  var async = false;
  var path = profilePath + "-does-not-exists";
  fs.readdir(path, function(error, entries) {
    assert.ok(async, "readdir is async");
    assert.equal(error.message, "ENOENT, readdir " + path);
    assert.equal(error.code, "ENOENT", "error has a code");
    assert.equal(error.path, path, "error has a path");
    assert.equal(error.errno, 34, "error has a errno");
    end();
  });

  async = true;
};

exports["test readdirSync"] = function(assert) {
  var async = false;
  var entries = fs.readdirSync(profilePath);
  assert.ok(profileEntries.length <= entries.length,
            "got at least number of entries we expect");
  assert.ok(profileEntries.every(function(entry) {
    return entries.indexOf(entry) >= 0;
  }), "all profiles are present");
};

exports["test readdirSync error"] = function(assert) {
  var async = false;
  var path = profilePath + "-does-not-exists";
  try {
    fs.readdirSync(path);
    assert.fail(Error("No error was thrown"));
  } catch (error) {
    assert.equal(error.message, "ENOENT, readdir " + path);
    assert.equal(error.code, "ENOENT", "error has a code");
    assert.equal(error.path, path, "error has a path");
    assert.equal(error.errno, 34, "error has a errno");
  }
};

exports["test readFile"] = function(assert, end) {
  let async = false;
  fs.readFile(filePathInProfile, function(error, content) {
    assert.ok(async, "readFile is async");
    assert.ok(!error, "error is falsy");

    assert.ok(Buffer.isBuffer(content), "readFile returns buffer");
    assert.ok(typeof(content.length) === "number", "buffer has length");
    assert.ok(content.toString().indexOf("[Compatibility]") >= 0,
              "content contains expected data");
    end();
  });
  async = true;
};

exports["test readFile error"] = function(assert, end) {
  let async = false;
  let path = filePathInProfile + "-does-not-exists";
  fs.readFile(path, function(error, content) {
    assert.ok(async, "readFile is async");
    assert.equal(error.message, "ENOENT, open " + path);
    assert.equal(error.code, "ENOENT", "error has a code");
    assert.equal(error.path, path, "error has a path");
    assert.equal(error.errno, 34, "error has a errno");

    end();
  });
  async = true;
};

exports["test readFileSync not implemented"] = function(assert) {
  let buffer = fs.readFileSync(filePathInProfile);
  assert.ok(buffer.toString().indexOf("[Compatibility]") >= 0,
            "read content");
};

exports["test fs.stat file"] = function(assert, end) {
  let async = false;
  let path = filePathInProfile;
  fs.stat(path, function(error, stat) {
    assert.ok(async, "fs.stat is async");
    assert.ok(!error, "error is falsy");
    assert.ok(!stat.isDirectory(), "not a dir");
    assert.ok(stat.isFile(), "is a file");
    assert.ok(!stat.isSymbolicLink(), "isn't a symlink");
    assert.ok(typeof(stat.size) === "number", "size is a number");
    assert.ok(stat.exists === true, "file exists");
    assert.ok(typeof(stat.isBlockDevice()) === "boolean");
    assert.ok(typeof(stat.isCharacterDevice()) === "boolean");
    assert.ok(typeof(stat.isFIFO()) === "boolean");
    assert.ok(typeof(stat.isSocket()) === "boolean");
    assert.ok(typeof(stat.hidden) === "boolean");
    assert.ok(typeof(stat.writable) === "boolean")
    assert.ok(stat.readable === true);

    end();
  });
  async = true;
};

exports["test fs.stat dir"] = function(assert, end) {
  let async = false;
  let path = dirPathInProfile;
  fs.stat(path, function(error, stat) {
    assert.ok(async, "fs.stat is async");
    assert.ok(!error, "error is falsy");
    assert.ok(stat.isDirectory(), "is a dir");
    assert.ok(!stat.isFile(), "not a file");
    assert.ok(!stat.isSymbolicLink(), "isn't a symlink");
    assert.ok(typeof(stat.size) === "number", "size is a number");
    assert.ok(stat.exists === true, "file exists");
    assert.ok(typeof(stat.isBlockDevice()) === "boolean");
    assert.ok(typeof(stat.isCharacterDevice()) === "boolean");
    assert.ok(typeof(stat.isFIFO()) === "boolean");
    assert.ok(typeof(stat.isSocket()) === "boolean");
    assert.ok(typeof(stat.hidden) === "boolean");
    assert.ok(typeof(stat.writable) === "boolean")
    assert.ok(typeof(stat.readable) === "boolean");

    end();
  });
  async = true;
};

exports["test fs.stat error"] = function(assert, end) {
  let async = false;
  let path = filePathInProfile + "-does-not-exists";
  fs.stat(path, function(error, stat) {
    assert.ok(async, "fs.stat is async");
    assert.equal(error.message, "ENOENT, stat " + path);
    assert.equal(error.code, "ENOENT", "error has a code");
    assert.equal(error.path, path, "error has a path");
    assert.equal(error.errno, 34, "error has a errno");

    end();
  });
  async = true;
};

exports["test fs.exists NO"] = function(assert, end) {
  let async = false
  let path = filePathInProfile + "-does-not-exists";
  fs.exists(path, function(error, value) {
    assert.ok(async, "fs.exists is async");
    assert.ok(!error, "error is falsy");
    assert.ok(!value, "file does not exists");
    end();
  });
  async = true;
};

exports["test fs.exists YES"] = function(assert, end) {
  let async = false
  let path = filePathInProfile
  fs.exists(path, function(error, value) {
    assert.ok(async, "fs.exists is async");
    assert.ok(!error, "error is falsy");
    assert.ok(value, "file exists");
    end();
  });
  async = true;
};

exports["test fs.exists NO"] = function(assert, end) {
  let async = false
  let path = filePathInProfile + "-does-not-exists";
  fs.exists(path, function(error, value) {
    assert.ok(async, "fs.exists is async");
    assert.ok(!error, "error is falsy");
    assert.ok(!value, "file does not exists");
    end();
  });
  async = true;
};

exports["test fs.existsSync"] = function(assert) {
  let path = filePathInProfile
  assert.equal(fs.existsSync(path), true, "exists");
  assert.equal(fs.existsSync(path + "-does-not-exists"), false, "exists");
};

exports["test fs.mkdirSync fs.rmdirSync"] = function(assert) {
  let path = mkdirPath;

  assert.equal(fs.existsSync(path), false, "does not exists");
  fs.mkdirSync(path);
  assert.equal(fs.existsSync(path), true, "dir was created");
  try {
    fs.mkdirSync(path);
    assert.fail(Error("mkdir on existing should throw"));
  } catch (error) {
    assert.equal(error.message, "EEXIST, mkdir " + path);
    assert.equal(error.code, "EEXIST", "error has a code");
    assert.equal(error.path, path, "error has a path");
    assert.equal(error.errno, 47, "error has a errno");
  }
  fs.rmdirSync(path);
  assert.equal(fs.existsSync(path), false, "dir was removed");
};

exports["test fs.mkdir"] = function(assert, end) {
  let path = mkdirPath;

  if (!fs.existsSync(path)) {
    let async = false;
    fs.mkdir(path, function(error) {
      assert.ok(async, "mkdir is async");
      assert.ok(!error, "no error");
      assert.equal(fs.existsSync(path), true, "dir was created");
      fs.rmdirSync(path);
      assert.equal(fs.existsSync(path), false, "dir was removed");
      end();
    });
    async = true;
  }
};

exports["test fs.mkdir error"] = function(assert, end) {
  let path = mkdirPath;

  if (!fs.existsSync(path)) {
    fs.mkdirSync(path);
    let async = false;
    fs.mkdir(path, function(error) {
      assert.ok(async, "mkdir is async");
      assert.equal(error.message, "EEXIST, mkdir " + path);
      assert.equal(error.code, "EEXIST", "error has a code");
      assert.equal(error.path, path, "error has a path");
      assert.equal(error.errno, 47, "error has a errno");
      fs.rmdirSync(path);
      assert.equal(fs.existsSync(path), false, "dir was removed");
      end();
    });
    async = true;
  }
};

exports["test fs.rmdir"] = function(assert, end) {
  let path = mkdirPath;

  if (!fs.existsSync(path)) {
    fs.mkdirSync(path);
    assert.equal(fs.existsSync(path), true, "dir exists");
    let async = false;
    fs.rmdir(path, function(error) {
      assert.ok(async, "mkdir is async");
      assert.ok(!error, "no error");
      assert.equal(fs.existsSync(path), false, "dir was removed");
      end();
    });
    async = true;
  }
};


exports["test fs.rmdir error"] = function(assert, end) {
  let path = mkdirPath;

  if (!fs.existsSync(path)) {
    assert.equal(fs.existsSync(path), false, "dir doesn't exists");
    let async = false;
    fs.rmdir(path, function(error) {
      assert.ok(async, "mkdir is async");
      assert.equal(error.message, "ENOENT, remove " + path);
      assert.equal(error.code, "ENOENT", "error has a code");
      assert.equal(error.path, path, "error has a path");
      assert.equal(error.errno, 34, "error has a errno");
      assert.equal(fs.existsSync(path), false, "dir is removed");
      end();
    });
    async = true;
  }
};

exports["test fs.truncateSync fs.unlinkSync"] = function(assert) {
  let path = truncatePath;

  assert.equal(fs.existsSync(path), false, "does not exists");
  fs.truncateSync(path);
  assert.equal(fs.existsSync(path), true, "file was created");
  fs.truncateSync(path);
  fs.unlinkSync(path);
  assert.equal(fs.existsSync(path), false, "file was removed");
};


exports["test fs.truncate"] = function(assert, end) {
  let path = truncatePath;
  if (!fs.existsSync(path)) {
    let async = false;
    fs.truncate(path, 0, function(error) {
      assert.ok(async, "truncate is async");
      assert.ok(!error, "no error");
      assert.equal(fs.existsSync(path), true, "file was created");
      fs.unlinkSync(path);
      assert.equal(fs.existsSync(path), false, "file was removed");
      end();
    })
    async = true;
  }
};

exports["test fs.unlink"] = function(assert, end) {
  let path = unlinkPath;
  let async = false;
  assert.ok(!fs.existsSync(path), "file doesn't exists yet");
  fs.truncateSync(path, 0);
  assert.ok(fs.existsSync(path), "file was created");
  fs.unlink(path, function(error) {
    assert.ok(async, "fs.unlink is async");
    assert.ok(!error, "error is falsy");
    assert.ok(!fs.existsSync(path), "file was removed");
    end();
  });
  async = true;
};

exports["test fs.unlink error"] = function(assert, end) {
  let path = unlinkPath;
  let async = false;
  assert.ok(!fs.existsSync(path), "file doesn't exists yet");
  fs.unlink(path, function(error) {
    assert.ok(async, "fs.unlink is async");
    assert.equal(error.message, "ENOENT, remove " + path);
    assert.equal(error.code, "ENOENT", "error has a code");
    assert.equal(error.path, path, "error has a path");
    assert.equal(error.errno, 34, "error has a errno");
    end();
  });
  async = true;
};

exports["test fs.rename"] = function(assert, end) {
  let fromPath = renameFromPath;
  let toPath = renameToPath;

  fs.truncateSync(fromPath);
  assert.ok(fs.existsSync(fromPath), "source file exists");
  assert.ok(!fs.existsSync(toPath), "destination doesn't exists");
  var async = false;
  fs.rename(fromPath, toPath, function(error) {
    assert.ok(async, "fs.rename is async");
    assert.ok(!error, "error is falsy");
    assert.ok(!fs.existsSync(fromPath), "source path no longer exists");
    assert.ok(fs.existsSync(toPath), "destination file exists");
    fs.unlinkSync(toPath);
    assert.ok(!fs.existsSync(toPath), "cleaned up properly");
    end();
  });
  async = true;
};

exports["test fs.rename (missing source file)"] = function(assert, end) {
  let fromPath = renameFromPath;
  let toPath = renameToPath;

  assert.ok(!fs.existsSync(fromPath), "source file doesn't exists");
  assert.ok(!fs.existsSync(toPath), "destination doesn't exists");
  var async = false;
  fs.rename(fromPath, toPath, function(error) {
    assert.ok(async, "fs.rename is async");
    assert.equal(error.message, "ENOENT, rename " + fromPath);
    assert.equal(error.code, "ENOENT", "error has a code");
    assert.equal(error.path, fromPath, "error has a path");
    assert.equal(error.errno, 34, "error has a errno");
    end();
  });
  async = true;
};

exports["test fs.rename (existing target file)"] = function(assert, end) {
  let fromPath = renameFromPath;
  let toPath = renameToPath;

  fs.truncateSync(fromPath);
  fs.truncateSync(toPath);
  assert.ok(fs.existsSync(fromPath), "source file exists");
  assert.ok(fs.existsSync(toPath), "destination file exists");
  var async = false;
  fs.rename(fromPath, toPath, function(error) {
    assert.ok(async, "fs.rename is async");
    assert.ok(!error, "error is falsy");
    assert.ok(!fs.existsSync(fromPath), "source path no longer exists");
    assert.ok(fs.existsSync(toPath), "destination file exists");
    fs.unlinkSync(toPath);
    assert.ok(!fs.existsSync(toPath), "cleaned up properly");
    end();
  });
  async = true;
};

exports["test fs.writeFile"] = function(assert, end) {
  let path = writePath;
  let content = ["hello world",
                 "this is some text"].join("\n");

  var async = false;
  fs.writeFile(path, content, function(error) {
    assert.ok(async, "fs write is async");
    assert.ok(!error, "error is falsy");
    assert.ok(fs.existsSync(path), "file was created");
    assert.equal(fs.readFileSync(path).toString(),
                 content,
                 "contet was written");
    fs.unlinkSync(path);
    assert.ok(!fs.exists(path), "file was removed");

    end();
  });
  async = true;
};

exports["test fs.writeFile (with large files)"] = function(assert, end) {
  let path = writePath;
  let content = "";

  for (var i = 0; i < 100000; i++) {
    content += "buffer\n";
  }

  var async = false;
  fs.writeFile(path, content, function(error) {
    assert.ok(async, "fs write is async");
    assert.ok(!error, "error is falsy");
    assert.ok(fs.existsSync(path), "file was created");
    assert.equal(fs.readFileSync(path).toString(),
                 content,
                 "contet was written");
    fs.unlinkSync(path);
    assert.ok(!fs.exists(path), "file was removed");

    end();
  });
  async = true;
};

exports["test fs.writeFile error"] = function (assert, done) {
  try {
    fs.writeFile({}, 'content', function (err) {
      assert.fail('Error thrown from TypeError should not be caught');
    });
  } catch (e) {
    assert.ok(e,
      'writeFile with a non-string error should not be caught');
    assert.equal(e.name, 'TypeError', 'error should be TypeError');
  }
  fs.writeFile('not/a/valid/path', 'content', function (err) {
    assert.ok(err, 'error caught and handled in callback');
    done();
  });
};

exports["test fs.chmod"] = function (assert, done) {
  let content = ["hej från sverige"];

  fs.writeFile(chmodPath, content, function (err) {
    testPerm("0755")()
      .then(testPerm("0777"))
      .then(testPerm("0666"))
      .then(testPerm(parseInt("0511", 8)))
      .then(testPerm(parseInt("0200", 8)))
      .then(testPerm("0040"))
      .then(testPerm("0000"))
      .then(testPermSync(parseInt("0777", 8)))
      .then(testPermSync(parseInt("0666", 8)))
      .then(testPermSync("0511"))
      .then(testPermSync("0200"))
      .then(testPermSync("0040"))
      .then(testPermSync("0000"))
      .then(() => {
        assert.pass("Successful chmod passes");
      }, assert.fail)
      // Test invalid paths
      .then(() => chmod("not-a-valid-file", parseInt("0755", 8)))
      .then(assert.fail, (err) => {
        checkPermError(err, "not-a-valid-file");
      })
      .then(() => chmod("not-a-valid-file", parseInt("0755", 8), "sync"))
      .then(assert.fail, (err) => {
        checkPermError(err, "not-a-valid-file");
      })
      // Test invalid files
      .then(() => chmod("resource://not-a-real-file", parseInt("0755", 8)))
      .then(assert.fail, (err) => {
        checkPermError(err, "resource://not-a-real-file");
      })
      .then(() => chmod("resource://not-a-real-file", parseInt("0755", 8), 'sync'))
      .then(assert.fail, (err) => {
        checkPermError(err, "resource://not-a-real-file");
      })
      .then(done, assert.fail);
  });

  function checkPermError (err, path) {
    assert.equal(err.message, "ENOENT, chmod " + path);
    assert.equal(err.code, "ENOENT", "error has a code");
    assert.equal(err.path, path, "error has a path");
    assert.equal(err.errno, 34, "error has a errno");
  }

  function chmod (path, mode, sync) {
    let { promise, resolve, reject } = defer();
    if (!sync) {
      fs.chmod(path, mode, (err) => {
        if (err) reject(err);
        else resolve();
      });
    } else {
      fs.chmodSync(path, mode);
      resolve();
    }
    return promise;
  }

  function testPerm (mode, sync) {
    return function () {
      return chmod(chmodPath, mode, sync)
        .then(() => getPerm(chmodPath))
        .then(perm => {
          let nMode = normalizeMode(mode);
          if (isWindows)
            assert.equal(perm, nMode,
              "mode correctly set to " + mode + " (" + nMode + " on windows)");
          else
            assert.equal(perm, nMode, "mode correctly set to " + nMode);
        });
    };
  }

  function testPermSync (mode) {
    return testPerm(mode, true);
  }

  function getPerm (path) {
    let { promise, resolve, reject } = defer();
    fs.stat(path, function (err, stat) {
      if (err) reject(err);
      else resolve(stat.mode);
    });
    return promise;
  }

  /*
   * Converts a unix mode `0755` into a Windows version of unix permissions
   */
  function normalizeMode (mode) {
    if (typeof mode === "string")
      mode = parseInt(mode, 8);

    if (!isWindows)
      return mode;

    var ANY_READ = parseInt("0444", 8);
    var ANY_WRITE = parseInt("0222", 8);
    var winMode = 0;

    // On Windows, if WRITE is true, then READ is also true
    if (mode & ANY_WRITE)
      winMode |= ANY_WRITE | ANY_READ;
    // Minimum permissions are READ for Windows
    else
      winMode |= ANY_READ;

    return winMode;
  }
};

require("test").run(exports);
