/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { dumpn } = require("devtools/shared/DevToolsUtils");

loader.lazyImporter(this, "OS", "resource://gre/modules/osfile.jsm");
loader.lazyImporter(
  this,
  "ExtensionParent",
  "resource://gre/modules/ExtensionParent.jsm"
);
loader.lazyRequireGetter(this, "Services");
loader.lazyRequireGetter(
  this,
  "FileUtils",
  "resource://gre/modules/FileUtils.jsm",
  true
);
loader.lazyRequireGetter(
  this,
  "NetUtil",
  "resource://gre/modules/NetUtil.jsm",
  true
);
loader.lazyGetter(this, "UNPACKED_ROOT_PATH", () => {
  return OS.Path.join(OS.Constants.Path.localProfileDir, "adb");
});
loader.lazyGetter(this, "EXTENSION_ID", () => {
  return Services.prefs.getCharPref("devtools.remote.adb.extensionID");
});
loader.lazyGetter(this, "ADB_BINARY_PATH", () => {
  let adbBinaryPath = OS.Path.join(UNPACKED_ROOT_PATH, "adb");
  if (Services.appinfo.OS === "WINNT") {
    adbBinaryPath += ".exe";
  }
  return adbBinaryPath;
});

const MANIFEST = "manifest.json";

/**
 * Read contents from a given uri in the devtools-adb-extension and parse the
 * contents as JSON.
 */
async function readFromExtension(fileUri) {
  return new Promise(resolve => {
    NetUtil.asyncFetch(
      {
        uri: fileUri,
        loadUsingSystemPrincipal: true,
      },
      input => {
        try {
          const string = NetUtil.readInputStreamToString(
            input,
            input.available()
          );
          resolve(JSON.parse(string));
        } catch (e) {
          dumpn(`Could not read ${fileUri} in the extension: ${e}`);
          resolve(null);
        }
      }
    );
  });
}

/**
 * Unpack file from the extension.
 * Uses NetUtil to read and write, since it's required for reading.
 *
 * @param {string} file
 *        The path name of the file in the extension.
 */
async function unpackFile(file) {
  const policy = ExtensionParent.WebExtensionPolicy.getByID(EXTENSION_ID);
  if (!policy) {
    return;
  }

  // Assumes that destination dir already exists.
  const basePath = file.substring(file.lastIndexOf("/") + 1);
  const filePath = OS.Path.join(UNPACKED_ROOT_PATH, basePath);
  await new Promise((resolve, reject) => {
    NetUtil.asyncFetch(
      {
        uri: policy.getURL(file),
        loadUsingSystemPrincipal: true,
      },
      input => {
        try {
          // Since we have to use NetUtil to read, probably it's okay to use for
          // writing, rather than bouncing to OS.File...?
          const outputFile = new FileUtils.File(filePath);
          const output = FileUtils.openAtomicFileOutputStream(outputFile);
          NetUtil.asyncCopy(input, output, resolve);
        } catch (e) {
          dumpn(`Could not unpack file ${file} in the extension: ${e}`);
          reject(e);
        }
      }
    );
  });
  // Mark binaries as executable.
  await OS.File.setPermissions(filePath, { unixMode: 0o744 });
}

/**
 * Extract files in the extension into local profile directory and returns
 * if it fails.
 */
async function extractFiles() {
  const policy = ExtensionParent.WebExtensionPolicy.getByID(EXTENSION_ID);
  if (!policy) {
    return false;
  }
  const uri = policy.getURL("adb.json");
  const adbInfo = await readFromExtension(uri);
  if (!adbInfo) {
    return false;
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

    // XPCOMABI looks this; x86_64-gcc3, so drop the compiler name.
    let architecture = Services.appinfo.XPCOMABI.split("-")[0];
    if (architecture === "aarch64") {
      // Fallback on x86 or x86_64 binaries for aarch64 - See Bug 1522149
      const hasX86Binary = !!adbInfo[Services.appinfo.OS].x86;
      architecture = hasX86Binary ? "x86" : "x86_64";
    }
    filesForAdb = adbInfo[Services.appinfo.OS][architecture];
  } catch (e) {
    return false;
  }

  // manifest.json isn't in adb.json but has to be unpacked for version
  // comparison
  filesForAdb.push(MANIFEST);

  await OS.File.makeDir(UNPACKED_ROOT_PATH);

  for (const file of filesForAdb) {
    try {
      await unpackFile(file);
    } catch (e) {
      return false;
    }
  }

  return true;
}

/**
 * Read the manifest from inside the devtools-adb-extension.
 * Uses NetUtil since data is packed inside the extension, not a local file.
 */
async function getManifestFromExtension() {
  const policy = ExtensionParent.WebExtensionPolicy.getByID(EXTENSION_ID);
  if (!policy) {
    return null;
  }

  const manifestUri = policy.getURL(MANIFEST);
  return readFromExtension(manifestUri);
}

/**
 * Returns whether manifest.json has already been unpacked.
 */
async function isManifestUnpacked() {
  const manifestPath = OS.Path.join(UNPACKED_ROOT_PATH, MANIFEST);
  return OS.File.exists(manifestPath);
}

/**
 * Read the manifest from the unpacked binary directory.
 * Uses OS.File since this is a local file.
 */
async function getManifestFromUnpacked() {
  if (!(await isManifestUnpacked())) {
    throw new Error("Manifest doesn't exist at unpacked path");
  }

  const manifestPath = OS.Path.join(UNPACKED_ROOT_PATH, MANIFEST);
  const binary = await OS.File.read(manifestPath);
  const json = new TextDecoder().decode(binary);
  let data;
  try {
    data = JSON.parse(json);
  } catch (e) {}
  return data;
}

/**
 * Check state of binary unpacking, including the location and manifest.
 */
async function isUnpacked() {
  if (!(await isManifestUnpacked())) {
    dumpn("Needs unpacking, no manifest found");
    return false;
  }

  const manifestInExtension = await getManifestFromExtension();
  const unpackedManifest = await getManifestFromUnpacked();
  if (manifestInExtension.version != unpackedManifest.version) {
    dumpn(
      `Needs unpacking, extension version ${manifestInExtension.version} != ` +
        `unpacked version ${unpackedManifest.version}`
    );
    return false;
  }
  dumpn("Already unpacked");
  return true;
}

/**
 * Get a file object for the adb binary from the 'adb@mozilla.org' extension
 * which has been already installed.
 *
 * @return {nsIFile}
 *        File object for the binary.
 */
async function getFileForBinary() {
  if (!(await isUnpacked()) && !(await extractFiles())) {
    return null;
  }

  const file = new FileUtils.File(ADB_BINARY_PATH);
  if (!file.exists()) {
    return null;
  }
  return file;
}

exports.getFileForBinary = getFileForBinary;
