/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

var { XPCOMUtils } = ChromeUtils.import(
  "resource://gre/modules/XPCOMUtils.jsm"
);
var { SessionWorker } = ChromeUtils.import(
  "resource:///modules/sessionstore/SessionWorker.jsm"
);
var { SessionWorkerCache } = ChromeUtils.import(
  "resource:///modules/sessionstore/SessionWorkerCache.jsm"
);

var File = OS.File;
var Paths;
var SessionFile;

// We need a XULAppInfo to initialize SessionFile
const { updateAppInfo } = ChromeUtils.import(
  "resource://testing-common/AppInfo.jsm"
);
updateAppInfo({
  name: "SessionRestoreTest",
  ID: "{230de50e-4cd1-11dc-8314-0800200c9a66}",
  version: "1",
  platformVersion: "",
});

add_task(async function init() {
  // Make sure that we have a profile before initializing SessionFile
  let profd = do_get_profile();
  SessionFile = ChromeUtils.import(
    "resource:///modules/sessionstore/SessionFile.jsm",
    {}
  ).SessionFile;
  Paths = SessionFile.Paths;

  let source = do_get_file("data/sessionstore_valid.js");
  source.copyTo(profd, "sessionstore.js");
  await writeCompressedFile(Paths.clean.replace("jsonlz4", "js"), Paths.clean);

  // Finish initialization of SessionFile
  await SessionFile.read();
});

function promise_check_contents(path, expect) {
  return (async function() {
    info("Checking whether " + path + " has the right contents");
    let actual = await OS.File.read(path, {
      encoding: "utf-8",
      compression: "lz4",
    });
    Assert.deepEqual(
      JSON.parse(actual),
      expect,
      `File ${path} contains the expected data.`
    );
  })();
}

function generateFileContents(id) {
  let url = `http://example.com/test_backup_once#${id}_${Math.random()}`;
  return {
    windows: [{ tabs: [{ entries: [{ url }], index: 1 }] }],
    _cachedObjs: [],
  };
}

add_task(async function test_writes_cache_objects() {
  let string = "Dummy string";
  let stringId = SessionWorkerCache.addRef(string);
  let content = generateFileContents("test_1");

  await File.makeDir(Paths.backups);
  await SessionFile.write(content);

  // Ensure that the session worker adds the cached object
  content._cachedObjs = [[stringId, string]];
  await promise_check_contents(Paths.recovery, content);

  // Ensure that once we release a reference to the cached object, the worker
  // no longer writes it
  SessionWorkerCache.release(string);
  content = generateFileContents("test_1");
  await SessionFile.write(content);
  await promise_check_contents(Paths.recovery, content);
});

add_task(async function test_rehydrates_cache_on_errors() {
  let string = "Dummy string";
  let stringId = SessionWorkerCache.addRef(string);
  let content = generateFileContents("test_1");

  await File.makeDir(Paths.backups);
  await SessionFile.write(content);

  // Reset the worker, and make sure that it still populates the cached object.
  SessionFile.resetWorker();
  content._cachedObjs = [[stringId, string]];
  await promise_check_contents(Paths.recovery, content);
});
