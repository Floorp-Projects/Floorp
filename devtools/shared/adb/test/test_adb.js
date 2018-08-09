"use strict";

const { ExtensionTestUtils } = ChromeUtils.import("resource://testing-common/ExtensionXPCShellUtils.jsm", {});
const { getFileForBinary } = require("devtools/shared/adb/adb-binary");

const ADB_JSON = {
  "Linux": {
    "x86": [
      "linux/adb"
    ],
    "x86_64": [
      "linux64/adb"
    ]
  },
  "Darwin": {
    "x86_64": [
      "mac64/adb"
    ]
  },
  "WINNT": {
    "x86": [
      "win32/adb.exe",
      "win32/AdbWinApi.dll",
      "win32/AdbWinUsbApi.dll"
    ],
    "x86_64": [
      "win32/adb.exe",
      "win32/AdbWinApi.dll",
      "win32/AdbWinUsbApi.dll"
    ]
  }
};

ExtensionTestUtils.init(this);

add_task(async function setup() {
  // Prepare the profile directory where the adb extension will be installed.
  do_get_profile();
});

add_task(async function testNoAdbExtension() {
  const extension = ExtensionTestUtils.loadExtension({
    manifest: {
      version: "1.0",
      applications: {
        gecko: { id: "not-adb@mozilla.org" }
      }
    },
  });

  await extension.startup();

  const adbBinary = await getFileForBinary();
  equal(adbBinary, null);

  await extension.unload();
});

add_task(async function testNoAdbJSON() {
  const extension = ExtensionTestUtils.loadExtension({
    manifest: {
      version: "1.0",
      applications: {
        gecko: { id: "adb@mozilla.org" }
      }
    },
  });

  await extension.startup();

  const adbBinary = await getFileForBinary();
  equal(adbBinary, null);

  await extension.unload();
});

add_task(async function testNoTargetBinaries() {
  const extension = ExtensionTestUtils.loadExtension({
    manifest: {
      version: "1.0",
      applications: {
        gecko: { id: "adb@mozilla.org" }
      }
    },
    files: {
      "adb.json": JSON.stringify(ADB_JSON),
    },
  });

  await extension.startup();

  const adbBinary = await getFileForBinary();
  equal(adbBinary, null);

  await extension.unload();
});

add_task(async function testExtract() {
  const extension = ExtensionTestUtils.loadExtension({
    manifest: {
      version: "1.0",
      applications: {
        gecko: { id: "adb@mozilla.org" }
      }
    },
    files: {
      "adb.json": JSON.stringify(ADB_JSON),
      "linux/adb": "adb",
      "linux64/adb": "adb",
      "mac64/adb": "adb",
      "win32/adb.exe": "adb.exe",
      "win32/AdbWinApi.dll": "AdbWinApi.dll",
      "win32/AdbWinUsbApi.dll": "AdbWinUsbApi.dll"
    },
  });

  await extension.startup();

  const adbBinary = await getFileForBinary();
  ok(await adbBinary.exists);

  await extension.unload();
});

