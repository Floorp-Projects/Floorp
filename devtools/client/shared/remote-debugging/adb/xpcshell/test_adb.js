/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const { ExtensionTestUtils } = ChromeUtils.import(
  "resource://testing-common/ExtensionXPCShellUtils.jsm"
);
const { NetUtil } = require("resource://gre/modules/NetUtil.jsm");
const {
  getFileForBinary,
} = require("devtools/client/shared/remote-debugging/adb/adb-binary");
const {
  check,
} = require("devtools/client/shared/remote-debugging/adb/adb-running-checker");
const {
  adbProcess,
} = require("devtools/client/shared/remote-debugging/adb/adb-process");
const {
  TrackDevicesCommand,
} = require("devtools/client/shared/remote-debugging/adb/commands/index");

const ADB_JSON = {
  Linux: {
    x86: ["linux/adb"],
    x86_64: ["linux64/adb"],
  },
  Darwin: {
    x86_64: ["mac64/adb"],
  },
  WINNT: {
    x86: ["win32/adb.exe", "win32/AdbWinApi.dll", "win32/AdbWinUsbApi.dll"],
    x86_64: ["win32/adb.exe", "win32/AdbWinApi.dll", "win32/AdbWinUsbApi.dll"],
  },
};
let extension_version = 1.0;

ExtensionTestUtils.init(this);

function readAdbMockContent() {
  const adbMockFile = do_get_file("adb.py", false);
  const s = Cc["@mozilla.org/network/file-input-stream;1"].createInstance(
    Ci.nsIFileInputStream
  );
  s.init(adbMockFile, -1, -1, false);
  try {
    return NetUtil.readInputStreamToString(s, s.available());
  } finally {
    s.close();
  }
}

const adbMock = readAdbMockContent();

add_task(async function setup() {
  // Prepare the profile directory where the adb extension will be installed.
  do_get_profile();
});

add_task(async function testAdbIsNotRunningInitially() {
  const isAdbRunning = await check();
  // Assume that no adb server running.
  ok(!isAdbRunning, "adb is not running initially");
});

add_task(async function testNoAdbExtension() {
  const extension = ExtensionTestUtils.loadExtension({
    manifest: {
      version: (extension_version++).toString(),
      applications: {
        gecko: { id: "not-adb@mozilla.org" },
      },
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
      version: (extension_version++).toString(),
      applications: {
        // The extension id here and in later test cases should match the
        // corresponding prefrece value.
        gecko: { id: "adb@mozilla.org" },
      },
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
      version: (extension_version++).toString(),
      applications: {
        gecko: { id: "adb@mozilla.org" },
      },
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
      version: (extension_version++).toString(),
      applications: {
        gecko: { id: "adb@mozilla.org" },
      },
    },
    files: {
      "adb.json": JSON.stringify(ADB_JSON),
      "linux/adb": "adb",
      "linux64/adb": "adb",
      "mac64/adb": "adb",
      "win32/adb.exe": "adb.exe",
      "win32/AdbWinApi.dll": "AdbWinApi.dll",
      "win32/AdbWinUsbApi.dll": "AdbWinUsbApi.dll",
    },
  });

  await extension.startup();

  const adbBinary = await getFileForBinary();
  ok(await adbBinary.exists());

  await extension.unload();
});

add_task(
  {
    skip_if: () => mozinfo.os == "win", // bug 1482008
  },
  async function testStartAndStop() {
    const extension = ExtensionTestUtils.loadExtension({
      manifest: {
        version: (extension_version++).toString(),
        applications: {
          gecko: { id: "adb@mozilla.org" },
        },
      },
      files: {
        "adb.json": JSON.stringify(ADB_JSON),
        "linux/adb": adbMock,
        "linux64/adb": adbMock,
        "mac64/adb": adbMock,
        "win32/adb.exe": adbMock,
        "win32/AdbWinApi.dll": "dummy",
        "win32/AdbWinUsbApi.dll": "dummy",
      },
    });

    await extension.startup();

    // Call start() once and call stop() afterwards.
    await adbProcess.start();
    ok(adbProcess.ready);
    ok(await check(), "adb is now running");

    await adbProcess.stop();
    ok(!adbProcess.ready);
    ok(!(await check()), "adb is no longer running");

    // Call start() twice and call stop() afterwards.
    await adbProcess.start();
    await adbProcess.start();
    ok(adbProcess.ready);
    ok(await check(), "adb is now running");

    await adbProcess.stop();
    ok(!adbProcess.ready);
    ok(!(await check()), "adb is no longer running");

    await extension.unload();
  }
);

add_task(
  {
    skip_if: () => mozinfo.os == "win", // bug 1482008
  },
  async function testTrackDevices() {
    const extension = ExtensionTestUtils.loadExtension({
      manifest: {
        version: (extension_version++).toString(),
        applications: {
          gecko: { id: "adb@mozilla.org" },
        },
      },
      files: {
        "adb.json": JSON.stringify(ADB_JSON),
        "linux/adb": adbMock,
        "linux64/adb": adbMock,
        "mac64/adb": adbMock,
        "win32/adb.exe": adbMock,
        "win32/AdbWinApi.dll": "dummy",
        "win32/AdbWinUsbApi.dll": "dummy",
      },
    });

    await extension.startup();

    await adbProcess.start();
    ok(adbProcess.ready);

    ok(await check(), "adb is now running");

    const receivedDeviceId = await new Promise(resolve => {
      const trackDevicesCommand = new TrackDevicesCommand();
      trackDevicesCommand.on("device-connected", deviceId => {
        resolve(deviceId);
      });
      trackDevicesCommand.run();
    });

    equal(receivedDeviceId, "1234567890");

    await adbProcess.stop();
    ok(!adbProcess.ready);

    await extension.unload();
  }
);
