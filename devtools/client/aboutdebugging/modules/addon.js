/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

loader.lazyImporter(this, "BrowserToolboxProcess",
  "resource://devtools/client/framework/ToolboxProcess.jsm");
loader.lazyImporter(this, "AddonManager", "resource://gre/modules/AddonManager.jsm");
loader.lazyImporter(this, "AddonManagerPrivate", "resource://gre/modules/AddonManager.jsm");

var {TargetFactory} = require("devtools/client/framework/target");
var {Toolbox} = require("devtools/client/framework/toolbox");

var {gDevTools} = require("devtools/client/framework/devtools");

let browserToolboxProcess = null;
let remoteAddonToolbox = null;
function closeToolbox() {
  if (browserToolboxProcess) {
    browserToolboxProcess.close();
  }

  if (remoteAddonToolbox) {
    remoteAddonToolbox.destroy();
  }
}

/**
 * Start debugging an addon in the current instance of Firefox.
 *
 * @param {String} addonID
 *        String id of the addon to debug.
 */
exports.debugLocalAddon = async function (addonID) {
  // Close previous addon debugging toolbox.
  closeToolbox();

  browserToolboxProcess = BrowserToolboxProcess.init({
    addonID,
    onClose: () => {
      browserToolboxProcess = null;
    }
  });
};

/**
 * Start debugging an addon in a remote instance of Firefox.
 *
 * @param {Object} addonForm
 *        Necessary to create an addon debugging target.
 * @param {DebuggerClient} client
 *        Required for remote debugging.
 */
exports.debugRemoteAddon = async function (addonForm, client) {
  // Close previous addon debugging toolbox.
  closeToolbox();

  let options = {
    form: addonForm,
    chrome: true,
    client,
    isTabActor: addonForm.isWebExtension
  };

  let target = await TargetFactory.forRemoteTab(options);

  let hostType = Toolbox.HostType.WINDOW;
  remoteAddonToolbox = await gDevTools.showToolbox(target, null, hostType);
  remoteAddonToolbox.once("destroy", () => {
    remoteAddonToolbox = null;
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
