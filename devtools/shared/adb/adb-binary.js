/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

loader.lazyImporter(this, "OS", "resource://gre/modules/osfile.jsm");
loader.lazyImporter(this, "ExtensionParent", "resource://gre/modules/ExtensionParent.jsm");
loader.lazyRequireGetter(this, "Services");
loader.lazyRequireGetter(this, "FileUtils",
                         "resource://gre/modules/FileUtils.jsm", true);
loader.lazyRequireGetter(this, "NetUtil",
                         "resource://gre/modules/NetUtil.jsm", true);
loader.lazyGetter(this, "UNPACKED_ROOT_PATH", () => {
  return OS.Path.join(OS.Constants.Path.localProfileDir, "adb");
});

// FIXME: Bug 1481691 - Read the extension ID from a pref.
const EXTENSION_ID = "adb@mozilla.org";

async function getAdbInfo(adbUri) {
  return new Promise(resolve => {
    NetUtil.asyncFetch({
      uri: adbUri,
      loadUsingSystemPrincipal: true
    }, (input) => {
      try {
        const string = NetUtil.readInputStreamToString(input, input.available());
        resolve(JSON.parse(string));
      } catch (e) {
        console.log(`Could not read adb.json in the extension: ${e}`);
        resolve(null);
      }
    });
  });
}

/**
 * Unpack file from the extension.
 * Uses NetUtil to read and write, since it's required for reading.
 *
 * @param {string} file
 *        The path name of the file in the extension, such as "win32/adb.exe".
 *        This has to have a '/' in the path string.
 */
async function unpackFile(file) {
  const policy = ExtensionParent.WebExtensionPolicy.getByID(EXTENSION_ID);
  if (!policy) {
    return;
  }

  // Assumes that destination dir already exists.
  const filePath = OS.Path.join(UNPACKED_ROOT_PATH, file.split("/")[1]);
  await new Promise((resolve, reject) => {
    NetUtil.asyncFetch({
      uri: policy.getURL(file),
      loadUsingSystemPrincipal: true
    }, (input) => {
      try {
        // Since we have to use NetUtil to read, probably it's okay to use for
        // writing, rather than bouncing to OS.File...?
        const outputFile = new FileUtils.File(filePath);
        const output = FileUtils.openAtomicFileOutputStream(outputFile);
        NetUtil.asyncCopy(input, output, resolve);
      } catch (e) {
        console.log(`Could not unpack file ${file} in the extension: ${e}`);
        reject(e);
      }
    });
  });
  // Mark binaries as executable.
  await OS.File.setPermissions(filePath, { unixMode: 0o744 });
}

/**
 * Extract files in the extension into local profile directory and returns
 * the path for the adb binary.
 */
async function extractBinary() {
  const policy = ExtensionParent.WebExtensionPolicy.getByID(EXTENSION_ID);
  if (!policy) {
    return null;
  }
  const uri = policy.getURL("adb.json");

  const adbInfo = await getAdbInfo(uri);
  if (!adbInfo) {
    return null;
  }

  let filesForAdb;
  try {
    // The adbInfo is an object looks like this;
    //
    //  {
    //    "Linux": {
    //      "x86": [
    //        "linux/adb"
    //      ],
    //      "x86_64": [
    //        "linux64/adb"
    //      ]
    //    },
    // ...
    filesForAdb =
      // XPCOMABI looks this; x86_64-gcc3, so drop the compiler name.
      adbInfo[Services.appinfo.OS][Services.appinfo.XPCOMABI.split("-")[0]];
  } catch (e) {
    return null;
  }

  await OS.File.makeDir(UNPACKED_ROOT_PATH);

  for (const file of filesForAdb) {
    try {
      await unpackFile(file);
    } catch (e) {
      return null;
    }
  }

  let adbBinaryPath = OS.Path.join(UNPACKED_ROOT_PATH, "adb");

  if (Services.appinfo.OS === "WINNT") {
    adbBinaryPath += ".exe";
  }
  return adbBinaryPath;
}

/**
 * Get a file object for the adb binary from the 'adb@mozilla.org' extension
 * which has been already installed.
 *
 * @return {nsIFile}
 *        File object for the binary.
 */
async function getFileForBinary() {
  const path = await extractBinary();
  if (!path) {
    return null;
  }
  console.log(`Binary path: ${path}`);
  return new FileUtils.File(path);
}
exports.getFileForBinary = getFileForBinary;
