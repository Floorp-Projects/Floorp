/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

loader.lazyImporter(this, "BrowserToolboxProcess",
  "resource://devtools/client/framework/ToolboxProcess.jsm");
loader.lazyImporter(this, "AddonManager", "resource://gre/modules/AddonManager.jsm");
loader.lazyImporter(this, "AddonManagerPrivate", "resource://gre/modules/AddonManager.jsm");

let toolbox = null;

exports.debugAddon = function (addonID) {
  if (toolbox) {
    toolbox.close();
  }

  toolbox = BrowserToolboxProcess.init({
    addonID,
    onClose: () => {
      toolbox = null;
    }
  });
};

exports.uninstallAddon = async function (addonID) {
  let addon = await AddonManager.getAddonByID(addonID);
  return addon && addon.uninstall();
};

exports.isTemporaryID = function (addonID) {
  return AddonManagerPrivate.isTemporaryInstallID(addonID);
};

exports.parseFileUri = function (url) {
  // Strip a leading slash from Windows drive letter URIs.
  // file:///home/foo ~> /home/foo
  // file:///C:/foo ~> C:/foo
  const windowsRegex = /^file:\/\/\/([a-zA-Z]:\/.*)/;
  if (windowsRegex.test(url)) {
    return windowsRegex.exec(url)[1];
  }
  return url.slice("file://".length);
};
