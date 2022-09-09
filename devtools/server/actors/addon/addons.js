/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { AddonManager } = require("resource://gre/modules/AddonManager.jsm");
const protocol = require("devtools/shared/protocol");
const { FileUtils } = require("resource://gre/modules/FileUtils.jsm");
const { addonsSpec } = require("devtools/shared/specs/addon/addons");

// This actor is not used by DevTools, but is relied on externally by
// webext-run and the Firefox VS-Code plugin. see bug #1578108
const AddonsActor = protocol.ActorClassWithSpec(addonsSpec, {
  initialize(conn) {
    protocol.Actor.prototype.initialize.call(this, conn);
  },

  async installTemporaryAddon(addonPath, openDevTools) {
    let addonFile;
    let addon;
    try {
      addonFile = new FileUtils.File(addonPath);
      addon = await AddonManager.installTemporaryAddon(addonFile);
    } catch (error) {
      throw new Error(`Could not install add-on at '${addonPath}': ${error}`);
    }

    Services.obs.notifyObservers(null, "devtools-installed-addon", addon.id);

    // Try to open DevTools for the installed add-on.
    // Note that it will only work on Firefox Desktop.
    // On Android, we don't ship DevTools UI.
    // about:debugging is only using this API when debugging its own firefox instance,
    // so for now, there is no chance of calling this on Android.
    if (openDevTools) {
      // eslint-disable-next-line mozilla/reject-some-requires
      const { gDevTools } = require("devtools/client/framework/devtools");
      gDevTools.showToolboxForWebExtension(addon.id);
    }

    // TODO: once the add-on actor has been refactored to use
    // protocol.js, we could return it directly.
    // return new AddonTargetActor(this.conn, addon);

    // Return a pseudo add-on object that a calling client can work
    // with. Provide a flag that the client can use to detect when it
    // gets upgraded to a real actor object.
    return { id: addon.id, actor: false };
  },
});

exports.AddonsActor = AddonsActor;
