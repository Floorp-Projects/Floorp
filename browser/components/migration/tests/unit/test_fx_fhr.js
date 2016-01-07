/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

var {FileUtils} = Cu.import("resource://gre/modules/FileUtils.jsm", {});

function run_test() {
  run_next_test();
}

function readFile(file) {
  let stream = Cc['@mozilla.org/network/file-input-stream;1']
               .createInstance(Ci.nsIFileInputStream);
  stream.init(file, -1, -1, Ci.nsIFileInputStream.CLOSE_ON_EOF);

  let sis = Cc["@mozilla.org/scriptableinputstream;1"]
            .createInstance(Ci.nsIScriptableInputStream);
  sis.init(stream);
  let contents = sis.read(file.fileSize);
  sis.close();
  return contents;
}

function checkDirectoryContains(dir, files) {
  print("checking " + dir.path + " - should contain " + Object.keys(files));
  let seen = new Set();
  let enumerator = dir.directoryEntries;
  while (enumerator.hasMoreElements()) {
    let file = enumerator.getNext().QueryInterface(Ci.nsIFile);
    print("found file: " + file.path);
    Assert.ok(file.leafName in files, file.leafName + " exists, but shouldn't");

    let expectedContents = files[file.leafName];
    if (typeof expectedContents != "string") {
      // it's a subdir - recurse!
      Assert.ok(file.isDirectory(), "should be a subdir");
      let newDir = dir.clone();
      newDir.append(file.leafName);
      checkDirectoryContains(newDir, expectedContents);
    } else {
      Assert.ok(!file.isDirectory(), "should be a regular file");
      let contents = readFile(file);
      Assert.equal(contents, expectedContents);
    }
    seen.add(file.leafName);
  }
  let missing = [];
  for (let x in files) {
    if (!seen.has(x)) {
      missing.push(x);
    }
  }
  Assert.deepEqual(missing, [], "no missing files in " + dir.path);
}

function getTestDirs() {
  // we make a directory structure in a temp dir which mirrors what we are
  // testing.
  let tempDir = do_get_tempdir();
  let srcDir = tempDir.clone();
  srcDir.append("test_source_dir");
  srcDir.createUnique(Ci.nsIFile.DIRECTORY_TYPE, FileUtils.PERMS_DIRECTORY);

  let targetDir = tempDir.clone();
  targetDir.append("test_target_dir");
  targetDir.createUnique(Ci.nsIFile.DIRECTORY_TYPE, FileUtils.PERMS_DIRECTORY);

  // no need to cleanup these dirs - the xpcshell harness will do it for us.
  return [srcDir, targetDir];
}

function writeToFile(dir, leafName, contents) {
  let file = dir.clone();
  file.append(leafName);

  let outputStream = FileUtils.openFileOutputStream(file);
  outputStream.write(contents, contents.length);
  outputStream.close();
}

function promiseMigrator(name, srcDir, targetDir) {
  let migrator = Cc["@mozilla.org/profile/migrator;1?app=browser&type=firefox"]
                 .createInstance(Ci.nsISupports)
                 .wrappedJSObject;
  let migrators = migrator._getResourcesInternal(srcDir, targetDir);
  for (let m of migrators) {
    if (m.name == name) {
      return new Promise((resolve, reject) => {
        m.migrate(resolve);
      });
    }
  }
  throw new Error("failed to find the " + name + " migrator");
}

function promiseFHRMigrator(srcDir, targetDir) {
  return promiseMigrator("healthreporter", srcDir, targetDir);
}

add_task(function* test_empty() {
  let [srcDir, targetDir] = getTestDirs();
  let ok = yield promiseFHRMigrator(srcDir, targetDir);
  Assert.ok(ok, "callback should have been true with empty directories");
  // check both are empty
  checkDirectoryContains(srcDir, {});
  checkDirectoryContains(targetDir, {});
});

add_task(function* test_just_sqlite() {
  let [srcDir, targetDir] = getTestDirs();

  let contents = "hello there\n\n";
  writeToFile(srcDir, "healthreport.sqlite", contents);

  let ok = yield promiseFHRMigrator(srcDir, targetDir);
  Assert.ok(ok, "callback should have been true with sqlite file copied");

  checkDirectoryContains(targetDir, {
    "healthreport.sqlite": contents,
  });
});

add_task(function* test_sqlite_extras() {
  let [srcDir, targetDir] = getTestDirs();

  let contents_sqlite = "hello there\n\n";
  writeToFile(srcDir, "healthreport.sqlite", contents_sqlite);

  let contents_wal = "this is the wal\n\n";
  writeToFile(srcDir, "healthreport.sqlite-wal", contents_wal);

  // and the -shm - this should *not* be copied.
  writeToFile(srcDir, "healthreport.sqlite-shm", "whatever");

  let ok = yield promiseFHRMigrator(srcDir, targetDir);
  Assert.ok(ok, "callback should have been true with sqlite file copied");

  checkDirectoryContains(targetDir, {
    "healthreport.sqlite": contents_sqlite,
    "healthreport.sqlite-wal": contents_wal,
  });
});

add_task(function* test_sqlite_healthreport_not_dir() {
  let [srcDir, targetDir] = getTestDirs();

  let contents = "hello there\n\n";
  writeToFile(srcDir, "healthreport.sqlite", contents);
  writeToFile(srcDir, "healthreport", "I'm a file but should be a directory");

  let ok = yield promiseFHRMigrator(srcDir, targetDir);
  Assert.ok(ok, "callback should have been true even though the directory was a file");
  // We should have only the sqlite file
  checkDirectoryContains(targetDir, {
    "healthreport.sqlite": contents,
  });
});

add_task(function* test_sqlite_healthreport_empty() {
  let [srcDir, targetDir] = getTestDirs();

  let contents = "hello there\n\n";
  writeToFile(srcDir, "healthreport.sqlite", contents);

  // create an empty 'healthreport' subdir.
  let subDir = srcDir.clone();
  subDir.append("healthreport");
  subDir.create(Ci.nsIFile.DIRECTORY_TYPE, FileUtils.PERMS_DIRECTORY);

  let ok = yield promiseFHRMigrator(srcDir, targetDir);
  Assert.ok(ok, "callback should have been true");

  // we should end up with the .sqlite file and an empty subdir in the target.
  checkDirectoryContains(targetDir, {
    "healthreport.sqlite": contents,
    "healthreport": {},
  });
});

add_task(function* test_sqlite_healthreport_contents() {
  let [srcDir, targetDir] = getTestDirs();

  let contents = "hello there\n\n";
  writeToFile(srcDir, "healthreport.sqlite", contents);

  // create an empty 'healthreport' subdir.
  let subDir = srcDir.clone();
  subDir.append("healthreport");
  subDir.create(Ci.nsIFile.DIRECTORY_TYPE, FileUtils.PERMS_DIRECTORY);

  writeToFile(subDir, "file1", "this is file 1");
  writeToFile(subDir, "file2", "this is file 2");

  let ok = yield promiseFHRMigrator(srcDir, targetDir);
  Assert.ok(ok, "callback should have been true");

  // we should end up with the .sqlite file and an empty subdir in the target.
  checkDirectoryContains(targetDir, {
    "healthreport.sqlite": contents,
    "healthreport": {
      "file1": "this is file 1",
      "file2": "this is file 2",
    },
  });
});

add_task(function* test_datareporting_empty() {
  let [srcDir, targetDir] = getTestDirs();

  // create an empty 'datareporting' subdir.
  let subDir = srcDir.clone();
  subDir.append("datareporting");
  subDir.create(Ci.nsIFile.DIRECTORY_TYPE, FileUtils.PERMS_DIRECTORY);

  let ok = yield promiseFHRMigrator(srcDir, targetDir);
  Assert.ok(ok, "callback should have been true");

  // we should end up with nothing at all in the destination - state.json was
  // missing so we didn't even create the target dir.
  checkDirectoryContains(targetDir, {});
});

add_task(function* test_datareporting_many() {
  let [srcDir, targetDir] = getTestDirs();

  // create an empty 'datareporting' subdir.
  let subDir = srcDir.clone();
  subDir.append("datareporting");
  subDir.create(Ci.nsIFile.DIRECTORY_TYPE, FileUtils.PERMS_DIRECTORY);

  writeToFile(subDir, "state.json", "should be copied");
  writeToFile(subDir, "something.else", "should not");

  let ok = yield promiseFHRMigrator(srcDir, targetDir);
  Assert.ok(ok, "callback should have been true");

  checkDirectoryContains(targetDir, {
    "datareporting" : {
      "state.json": "should be copied",
    }
  });
});

add_task(function* test_times_migration() {
  let [srcDir, targetDir] = getTestDirs();

  // create a times.json in the source directory.
  let contents = JSON.stringify({created: 1234});
  writeToFile(srcDir, "times.json", contents);

  let earliest = Date.now();
  let ok = yield promiseMigrator("times", srcDir, targetDir);
  Assert.ok(ok, "callback should have been true");
  let latest = Date.now();

  let timesFile = targetDir.clone();
  timesFile.append("times.json");

  let raw = readFile(timesFile);
  let times = JSON.parse(raw);
  Assert.ok(times.reset >= earliest && times.reset <= latest);
  // and it should have left the creation time alone.
  Assert.equal(times.created, 1234);
});
