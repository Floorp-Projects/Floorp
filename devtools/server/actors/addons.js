/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const {AddonManager} = require("resource://gre/modules/AddonManager.jsm");
const protocol = require("devtools/shared/protocol");
const {FileUtils} = require("resource://gre/modules/FileUtils.jsm");
const {Task} = require("devtools/shared/task");
const {addonsSpec} = require("devtools/shared/specs/addons");

const AddonsActor = protocol.ActorClassWithSpec(addonsSpec, {

  initialize: function (conn) {
    protocol.Actor.prototype.initialize.call(this, conn);
  },

  installTemporaryAddon: Task.async(function* (addonPath) {
    let addonFile;
    let addon;
    try {
      addonFile = new FileUtils.File(addonPath);
      addon = yield AddonManager.installTemporaryAddon(addonFile);
    } catch (error) {
      throw new Error(`Could not install add-on at '${addonPath}': ${error}`);
    }

    // TODO: once the add-on actor has been refactored to use
    // protocol.js, we could return it directly.
    // return new BrowserAddonActor(this.conn, addon);

    // Return a pseudo add-on object that a calling client can work
    // with. Provide a flag that the client can use to detect when it
    // gets upgraded to a real actor object.
    return { id: addon.id, actor: false };

  }),
});

exports.AddonsActor = AddonsActor;
