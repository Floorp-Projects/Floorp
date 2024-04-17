/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

import { AppConstants } from "resource://gre/modules/AppConstants.sys.mjs";

const lazy = {};
ChromeUtils.defineESModuleGetters(lazy, {
  ObjectUtils: "resource://gre/modules/ObjectUtils.sys.mjs",
});

/**
 * Default implementation of the helper class to assist in deleting the firefox protocols.
 * See maybeDeleteBridgeProtocolRegistryEntries for more info.
 */
class DeleteBridgeProtocolRegistryEntryHelperImplementation {
  getApplicationPath() {
    return Services.dirsvc.get("XREExeF", Ci.nsIFile).path;
  }

  openRegistryRoot() {
    const wrk = Cc["@mozilla.org/windows-registry-key;1"].createInstance(
      Ci.nsIWindowsRegKey
    );

    wrk.open(wrk.ROOT_KEY_CURRENT_USER, "Software\\Classes", wrk.ACCESS_ALL);

    return wrk;
  }

  deleteChildren(start) {
    // Recursively delete all of the children of the children
    // Go through the list in reverse order, so that shrinking
    // the list doesn't rearrange things while iterating
    for (let i = start.childCount; i > 0; i--) {
      const childName = start.getChildName(i - 1);
      const child = start.openChild(childName, start.ACCESS_ALL);
      this.deleteChildren(child);
      child.close();

      start.removeChild(childName);
    }
  }

  deleteRegistryTree(root, toDeletePath) {
    var start = root.openChild(toDeletePath, root.ACCESS_ALL);
    this.deleteChildren(start);
    start.close();

    root.removeChild(toDeletePath);
  }
}

export const FirefoxBridgeExtensionUtils = {
  /**
   * In Firefox 122, we enabled the firefox and firefox-private protocols.
   * We switched over to using firefox-bridge and firefox-private-bridge,
   *
   * In Firefox 126, we deleted the above firefox-bridge and
   * firefox-private-bridge protocols in favor of using native
   * messaging so we are only keeping the deletion code.
   *
   * but we want to clean up the use of the other protocols.
   *
   * deleteBridgeProtocolRegistryEntryHelper handles everything outside of the logic needed for
   * this method so that the logic in maybeDeleteBridgeProtocolRegistryEntries can be unit tested
   *
   * We only delete the entries for the firefox and firefox-private protocols if
   * they were set up to use this install and in the format that Firefox installed
   * them with. If the entries are changed in any way, it is assumed that the user
   * mucked with them manually and knows what they are doing.
   */

  PUBLIC_PROTOCOL: "firefox-bridge",
  PRIVATE_PROTOCOL: "firefox-private-bridge",
  OLD_PUBLIC_PROTOCOL: "firefox",
  OLD_PRIVATE_PROTOCOL: "firefox-private",

  maybeDeleteBridgeProtocolRegistryEntries(
    publicProtocol = this.PUBLIC_PROTOCOL,
    privateProtocol = this.PRIVATE_PROTOCOL,
    deleteBridgeProtocolRegistryEntryHelper = new DeleteBridgeProtocolRegistryEntryHelperImplementation()
  ) {
    try {
      var wrk = deleteBridgeProtocolRegistryEntryHelper.openRegistryRoot();
      const path = deleteBridgeProtocolRegistryEntryHelper.getApplicationPath();

      const maybeDeleteRegistryKey = (protocol, protocolCommand) => {
        const openCommandPath = protocol + "\\shell\\open\\command";
        if (wrk.hasChild(openCommandPath)) {
          let deleteProtocolEntry = false;

          try {
            var openCommandKey = wrk.openChild(
              openCommandPath,
              wrk.ACCESS_READ
            );
            if (openCommandKey.valueCount == 1) {
              const defaultKeyName = "";
              if (openCommandKey.getValueName(0) == defaultKeyName) {
                if (
                  openCommandKey.getValueType(defaultKeyName) ==
                  Ci.nsIWindowsRegKey.TYPE_STRING
                ) {
                  const val = openCommandKey.readStringValue(defaultKeyName);
                  if (val == protocolCommand) {
                    deleteProtocolEntry = true;
                  }
                }
              }
            }
          } finally {
            openCommandKey.close();
          }

          if (deleteProtocolEntry) {
            deleteBridgeProtocolRegistryEntryHelper.deleteRegistryTree(
              wrk,
              protocol
            );
          }
        }
      };

      maybeDeleteRegistryKey(publicProtocol, `\"${path}\" -osint -url \"%1\"`);
      maybeDeleteRegistryKey(
        privateProtocol,
        `\"${path}\" -osint -private-window \"%1\"`
      );
    } catch (err) {
      console.error(err);
    } finally {
      wrk.close();
    }
  },

  getNativeMessagingHostId() {
    let nativeMessagingHostId = "org.mozilla.firefox_bridge_nmh";
    if (AppConstants.NIGHTLY_BUILD) {
      nativeMessagingHostId += "_nightly";
    } else if (AppConstants.MOZ_DEV_EDITION) {
      nativeMessagingHostId += "_dev";
    } else if (AppConstants.IS_ESR) {
      nativeMessagingHostId += "_esr";
    }
    return nativeMessagingHostId;
  },

  getExtensionOrigins() {
    return Services.prefs
      .getStringPref("browser.firefoxbridge.extensionOrigins", "")
      .split(",");
  },

  async maybeWriteManifestFiles(
    nmhManifestFolder,
    nativeMessagingHostId,
    dualBrowserExtensionOrigins
  ) {
    try {
      let binFile = Services.dirsvc.get("XREExeF", Ci.nsIFile).parent;
      if (AppConstants.platform == "win") {
        binFile.append("nmhproxy.exe");
      } else if (AppConstants.platform == "macosx") {
        binFile.append("nmhproxy");
      } else {
        throw new Error("Unsupported platform");
      }

      let jsonContent = {
        name: nativeMessagingHostId,
        description: "Firefox Native Messaging Host",
        path: binFile.path,
        type: "stdio",
        allowed_origins: dualBrowserExtensionOrigins,
      };
      let nmhManifestFile = await IOUtils.getFile(
        nmhManifestFolder,
        `${nativeMessagingHostId}.json`
      );

      // This throws an error if the JSON file doesn't exist
      // or if it's corrupt.
      let correctFileExists = true;
      try {
        correctFileExists = lazy.ObjectUtils.deepEqual(
          await IOUtils.readJSON(nmhManifestFile.path),
          jsonContent
        );
      } catch (e) {
        correctFileExists = false;
      }
      if (!correctFileExists) {
        await IOUtils.writeJSON(nmhManifestFile.path, jsonContent);
      }
    } catch (e) {
      console.error(e);
    }
  },

  async ensureRegistered() {
    let nmhManifestFolder = null;
    if (AppConstants.platform == "win") {
      // We don't have permission to write to the application install directory
      // so instead write to %AppData%\Mozilla\Firefox.
      nmhManifestFolder = PathUtils.join(
        Services.dirsvc.get("AppData", Ci.nsIFile).path,
        "Mozilla",
        "Firefox"
      );
    } else if (AppConstants.platform == "macosx") {
      nmhManifestFolder =
        "~/Library/Application Support/Google/Chrome/NativeMessagingHosts/";
    } else {
      throw new Error("Unsupported platform");
    }
    await this.maybeWriteManifestFiles(
      nmhManifestFolder,
      this.getNativeMessagingHostId(),
      this.getExtensionOrigins()
    );
    if (AppConstants.platform == "win") {
      this.maybeWriteNativeMessagingRegKeys(
        "Software\\Google\\Chrome\\NativeMessagingHosts",
        nmhManifestFolder,
        this.getNativeMessagingHostId()
      );
    }
  },

  maybeWriteNativeMessagingRegKeys(
    regPath,
    nmhManifestFolder,
    NATIVE_MESSAGING_HOST_ID
  ) {
    let wrk = Cc["@mozilla.org/windows-registry-key;1"].createInstance(
      Ci.nsIWindowsRegKey
    );
    try {
      let expectedValue = PathUtils.join(
        nmhManifestFolder,
        `${NATIVE_MESSAGING_HOST_ID}.json`
      );
      try {
        // If the key already exists it will just be opened
        wrk.create(
          wrk.ROOT_KEY_CURRENT_USER,
          regPath + `\\${NATIVE_MESSAGING_HOST_ID}`,
          wrk.ACCESS_ALL
        );
        if (wrk.readStringValue("") == expectedValue) {
          return;
        }
      } catch (e) {
        // The key either doesn't have a value or doesn't exist
        // In either case we need to write it.
      }
      wrk.writeStringValue("", expectedValue);
    } catch (e) {
      // The method fails if we can't access the key
      // which means it doesn't exist. That's a normal situation.
      // We don't need to do anything here.
    } finally {
      wrk.close();
    }
  },
};
