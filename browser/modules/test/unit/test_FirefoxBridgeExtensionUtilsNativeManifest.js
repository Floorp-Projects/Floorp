/* Any copyright is dedicated to the Public Domain.
http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const { AppConstants } = ChromeUtils.importESModule(
  "resource://gre/modules/AppConstants.sys.mjs"
);
const { FileUtils } = ChromeUtils.importESModule(
  "resource://gre/modules/FileUtils.sys.mjs"
);
const { FirefoxBridgeExtensionUtils } = ChromeUtils.importESModule(
  "resource:///modules/FirefoxBridgeExtensionUtils.sys.mjs"
);

const DUAL_BROWSER_EXTENSION_ORIGIN = ["chrome-extension://fake-origin/"];
const NATIVE_MESSAGING_HOST_ID = "org.mozilla.firefox_bridge_test";

if (AppConstants.platform == "win") {
  var { MockRegistry } = ChromeUtils.importESModule(
    "resource://testing-common/MockRegistry.sys.mjs"
  );
}

let registry = null;
add_setup(() => {
  if (AppConstants.platform == "win") {
    registry = new MockRegistry();
    registerCleanupFunction(() => {
      registry.shutdown();
    });
  }
});

function resetMockRegistry() {
  if (AppConstants.platform != "win") {
    return;
  }
  registry.shutdown();
  registry = new MockRegistry();
}

let dir = FileUtils.getDir("TmpD", ["NativeMessagingHostsTest"]);
dir.createUnique(Ci.nsIFile.DIRECTORY_TYPE, FileUtils.PERMS_DIRECTORY);

let userDir = dir.clone();
userDir.append("user");
userDir.create(Ci.nsIFile.DIRECTORY_TYPE, FileUtils.PERMS_DIRECTORY);

let appDir = dir.clone();
appDir.append("app");
appDir.create(Ci.nsIFile.DIRECTORY_TYPE, FileUtils.PERMS_DIRECTORY);

let dirProvider = {
  getFile(property) {
    if (property == "Home") {
      return userDir.clone();
    } else if (property == "AppData") {
      return appDir.clone();
    }
    return null;
  },
};

try {
  Services.dirsvc.undefine("Home");
} catch (e) {}
try {
  Services.dirsvc.undefine("AppData");
} catch (e) {}
Services.dirsvc.registerProvider(dirProvider);

registerCleanupFunction(() => {
  Services.dirsvc.unregisterProvider(dirProvider);
  dir.remove(true);
});

const USER_TEST_PATH = PathUtils.join(userDir.path, "manifestDir");

let binFile = null;
add_setup(async function () {
  binFile = Services.dirsvc.get("XREExeF", Ci.nsIFile).parent.clone();
  if (AppConstants.platform == "win") {
    binFile.append("nmhproxy.exe");
  } else if (AppConstants.platform == "macosx") {
    binFile.append("nmhproxy");
  } else {
    throw new Error("Unsupported platform");
  }
});

function getExpectedOutput() {
  return {
    name: NATIVE_MESSAGING_HOST_ID,
    description: "Firefox Native Messaging Host",
    path: binFile.path,
    type: "stdio",
    allowed_origins: DUAL_BROWSER_EXTENSION_ORIGIN,
  };
}

function DumpWindowsRegistry() {
  let key = "";
  let pathBuffer = [];

  if (AppConstants.platform == "win") {
    function bufferPrint(line) {
      let regPath = line.trimStart();
      if (regPath.includes(":")) {
        // After trimming white space, keys are formatted as
        // ": <key> (<value_type>)". We can assume it's only ever
        // going to be of type REG_SZ for this test.
        key = regPath.slice(2, regPath.length - " (REG_SZ)".length);
      } else {
        pathBuffer.push(regPath);
      }
    }

    MockRegistry.dump(
      MockRegistry.getRoot(Ci.nsIWindowsRegKey.ROOT_KEY_CURRENT_USER),
      "",
      bufferPrint
    );
  } else {
    Assert.ok(false, "Only windows has a registry!");
  }

  return [pathBuffer, key];
}

add_task(async function test_maybeWriteManifestFiles() {
  await FirefoxBridgeExtensionUtils.maybeWriteManifestFiles(
    USER_TEST_PATH,
    NATIVE_MESSAGING_HOST_ID,
    DUAL_BROWSER_EXTENSION_ORIGIN
  );
  let expectedOutput = JSON.stringify(getExpectedOutput());
  let nmhManifestFilePath = PathUtils.join(
    USER_TEST_PATH,
    `${NATIVE_MESSAGING_HOST_ID}.json`
  );
  let nmhManifestFileContent = await IOUtils.readUTF8(nmhManifestFilePath);
  await IOUtils.remove(nmhManifestFilePath);
  Assert.equal(nmhManifestFileContent, expectedOutput);
});

add_task(async function test_maybeWriteManifestFilesIncorrect() {
  let nmhManifestFile = await IOUtils.getFile(
    USER_TEST_PATH,
    `${NATIVE_MESSAGING_HOST_ID}.json`
  );

  let incorrectInput = {
    name: NATIVE_MESSAGING_HOST_ID,
    description: "Manifest with unexpected description",
    path: binFile.path,
    type: "stdio",
    allowed_origins: DUAL_BROWSER_EXTENSION_ORIGIN,
  };
  await IOUtils.writeJSON(nmhManifestFile.path, incorrectInput);

  // Write correct JSON to the file and check to make sure it matches
  // the expected output
  await FirefoxBridgeExtensionUtils.maybeWriteManifestFiles(
    USER_TEST_PATH,
    NATIVE_MESSAGING_HOST_ID,
    DUAL_BROWSER_EXTENSION_ORIGIN
  );
  let expectedOutput = JSON.stringify(getExpectedOutput());

  let nmhManifestFilePath = PathUtils.join(
    USER_TEST_PATH,
    `${NATIVE_MESSAGING_HOST_ID}.json`
  );
  let nmhManifestFileContent = await IOUtils.readUTF8(nmhManifestFilePath);
  await IOUtils.remove(nmhManifestFilePath);
  Assert.equal(nmhManifestFileContent, expectedOutput);
});

add_task(async function test_maybeWriteManifestFilesAlreadyExists() {
  // Write file and confirm it exists
  await FirefoxBridgeExtensionUtils.maybeWriteManifestFiles(
    USER_TEST_PATH,
    NATIVE_MESSAGING_HOST_ID,
    DUAL_BROWSER_EXTENSION_ORIGIN
  );
  let nmhManifestFile = await IOUtils.getFile(
    USER_TEST_PATH,
    `${NATIVE_MESSAGING_HOST_ID}.json`
  );

  // Modify file modificatiomn time to be older than the write time
  let oldModificationTime = Date.now() - 1000000;
  let setModificationTime = await IOUtils.setModificationTime(
    nmhManifestFile.path,
    oldModificationTime
  );
  Assert.equal(oldModificationTime, setModificationTime);

  // Call function which writes correct JSON to the file and make sure
  // the modification time is the same, meaning we haven't written anything
  await FirefoxBridgeExtensionUtils.maybeWriteManifestFiles(
    USER_TEST_PATH,
    NATIVE_MESSAGING_HOST_ID,
    DUAL_BROWSER_EXTENSION_ORIGIN
  );
  let stat = await IOUtils.stat(nmhManifestFile.path);
  await IOUtils.remove(nmhManifestFile.path);
  Assert.equal(stat.lastModified, oldModificationTime);
});

add_task(async function test_maybeWriteManifestFilesDirDoesNotExist() {
  let testDir = dir.clone();
  // This folder does not exist, so we want to make sure it's created
  testDir.append("dirDoesNotExist");
  await FirefoxBridgeExtensionUtils.maybeWriteManifestFiles(
    testDir.path,
    NATIVE_MESSAGING_HOST_ID,
    DUAL_BROWSER_EXTENSION_ORIGIN
  );

  ok(await IOUtils.exists(testDir.path));
  ok(
    await IOUtils.exists(
      PathUtils.join(testDir.path, `${NATIVE_MESSAGING_HOST_ID}.json`)
    )
  );
  await IOUtils.remove(testDir.path, { recursive: true });
});

add_task(async function test_ensureRegistered() {
  let expectedJSONDirPath = null;
  let nativeHostId = "org.mozilla.firefox_bridge_nmh";
  if (AppConstants.NIGHTLY_BUILD) {
    nativeHostId = "org.mozilla.firefox_bridge_nmh_nightly";
  } else if (AppConstants.MOZ_DEV_EDITION) {
    nativeHostId = "org.mozilla.firefox_bridge_nmh_dev";
  } else if (AppConstants.IS_ESR) {
    nativeHostId = "org.mozilla.firefox_bridge_nmh_esr";
  }

  if (AppConstants.platform == "macosx") {
    expectedJSONDirPath = PathUtils.joinRelative(
      userDir.path,
      "Library/Application Support/Google/Chrome/NativeMessagingHosts/"
    );
  } else if (AppConstants.platform == "win") {
    expectedJSONDirPath = PathUtils.joinRelative(
      appDir.path,
      "Mozilla\\Firefox"
    );
    resetMockRegistry();
  } else {
    throw new Error("Unsupported platform");
  }

  ok(!(await IOUtils.exists(expectedJSONDirPath)));
  let expectedJSONPath = PathUtils.join(
    expectedJSONDirPath,
    `${nativeHostId}.json`
  );

  await FirefoxBridgeExtensionUtils.ensureRegistered();
  let realOutput = {
    name: nativeHostId,
    description: "Firefox Native Messaging Host",
    path: binFile.path,
    type: "stdio",
    allowed_origins: FirefoxBridgeExtensionUtils.getExtensionOrigins(),
  };

  let expectedOutput = JSON.stringify(realOutput);
  let JSONContent = await IOUtils.readUTF8(expectedJSONPath);
  await IOUtils.remove(expectedJSONPath);
  Assert.equal(JSONContent, expectedOutput);

  //  Test that the registry key is written for Windows only
  if (AppConstants.platform == "win") {
    let [pathBuffer, key] = DumpWindowsRegistry();
    Assert.equal(
      pathBuffer.toString(),
      [
        "Software",
        "Google",
        "Chrome",
        "NativeMessagingHosts",
        nativeHostId,
      ].toString()
    );
    Assert.equal(key, expectedJSONPath);
  }
});

add_task(async function test_maybeWriteNativeMessagingRegKeys() {
  if (AppConstants.platform != "win") {
    return;
  }
  resetMockRegistry();
  FirefoxBridgeExtensionUtils.maybeWriteNativeMessagingRegKeys(
    "Test\\Path\\For\\Reg\\Key",
    binFile.parent.path,
    NATIVE_MESSAGING_HOST_ID
  );
  let [pathBuffer, key] = DumpWindowsRegistry();
  registry.shutdown();
  Assert.equal(
    pathBuffer.toString(),
    ["Test", "Path", "For", "Reg", "Key", NATIVE_MESSAGING_HOST_ID].toString()
  );
  console.log("The key is: " + key);
  Assert.equal(key, `${binFile.parent.path}\\${NATIVE_MESSAGING_HOST_ID}.json`);
});

add_task(async function test_maybeWriteNativeMessagingRegKeysIncorrectValue() {
  if (AppConstants.platform != "win") {
    return;
  }
  resetMockRegistry();
  registry.setValue(
    Ci.nsIWindowsRegKey.ROOT_KEY_CURRENT_USER,
    `Test\\Path\\For\\Reg\\Key\\${NATIVE_MESSAGING_HOST_ID}`,
    "",
    "IncorrectValue"
  );
  FirefoxBridgeExtensionUtils.maybeWriteNativeMessagingRegKeys(
    "Test\\Path\\For\\Reg\\Key",
    binFile.parent.path,
    NATIVE_MESSAGING_HOST_ID
  );
  let [pathBuffer, key] = DumpWindowsRegistry();
  registry.shutdown();
  Assert.equal(
    pathBuffer.toString(),
    ["Test", "Path", "For", "Reg", "Key", NATIVE_MESSAGING_HOST_ID].toString()
  );
  Assert.equal(key, `${binFile.parent.path}\\${NATIVE_MESSAGING_HOST_ID}.json`);
});
