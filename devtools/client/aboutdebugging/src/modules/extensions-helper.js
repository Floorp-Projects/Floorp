/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { Cc, Ci } = require("chrome");
loader.lazyImporter(
  this,
  "AddonManager",
  "resource://gre/modules/AddonManager.jsm"
);
loader.lazyRequireGetter(
  this,
  "FileUtils",
  "resource://gre/modules/FileUtils.jsm",
  true
);

const { PREFERENCES } = require("devtools/client/aboutdebugging/src/constants");

/**
 * Uninstall the addon with the provided id.
 * Resolves when the addon shutdown has completed.
 */
exports.uninstallAddon = async function(addonID) {
  const addon = await AddonManager.getAddonByID(addonID);
  return addon && addon.uninstall();
};

exports.parseFileUri = function(url) {
  // Strip a leading slash from Windows drive letter URIs.
  // file:///home/foo ~> /home/foo
  // file:///C:/foo ~> C:/foo
  const windowsRegex = /^file:\/\/\/([a-zA-Z]:\/.*)/;
  if (windowsRegex.test(url)) {
    return windowsRegex.exec(url)[1];
  }
  return url.slice("file://".length);
};

exports.getExtensionUuid = function(extension) {
  const { manifestURL } = extension;
  // Strip off the protocol and rest, leaving us with just the UUID.
  return manifestURL ? /moz-extension:\/\/([^/]*)/.exec(manifestURL)[1] : null;
};

/**
 * Open a file picker to allow the user to locate a temporary extension. A temporary
 * extension can either be:
 * - a folder
 * - a .xpi file
 * - a .zip file
 *
 * @param  {Window} win
 *         The window object where the filepicker should be opened.
 *         Note: We cannot use the global window object here because it is undefined if
 *         this module is loaded from a file outside of devtools/client/aboutdebugging/.
 *         See browser-loader.js `uri.startsWith(baseURI)` for more details.
 * @param  {String} message
 *         The help message that should be displayed to the user in the filepicker.
 * @return {Promise} returns a promise that resolves a File object corresponding to the
 *         file selected by the user.
 */
exports.openTemporaryExtension = function(win, message) {
  return new Promise(resolve => {
    const fp = Cc["@mozilla.org/filepicker;1"].createInstance(Ci.nsIFilePicker);
    fp.init(win, message, Ci.nsIFilePicker.modeOpen);

    // Try to set the last directory used as "displayDirectory".
    try {
      const lastDirPath = Services.prefs.getCharPref(
        PREFERENCES.TEMPORARY_EXTENSION_PATH,
        ""
      );
      const lastDir = new FileUtils.File(lastDirPath);
      fp.displayDirectory = lastDir;
    } catch (e) {
      // Empty or invalid value, nothing to handle.
    }

    fp.open(res => {
      if (res == Ci.nsIFilePicker.returnCancel || !fp.file) {
        return;
      }
      let file = fp.file;
      // AddonManager.installTemporaryAddon accepts either
      // addon directory or final xpi file.
      if (
        !file.isDirectory() &&
        !file.leafName.endsWith(".xpi") &&
        !file.leafName.endsWith(".zip")
      ) {
        file = file.parent;
      }

      // We are about to resolve, store the path to the file for the next call.
      Services.prefs.setCharPref(
        PREFERENCES.TEMPORARY_EXTENSION_PATH,
        file.path
      );

      resolve(file);
    });
  });
};
