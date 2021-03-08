/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { createCommandsDictionary } = require("devtools/shared/commands/index");

/**
 * A Descriptor represents a debuggable context. It can be a browser tab, a tab on
 * a remote device, like a tab on Firefox for Android. But it can also be an add-on,
 * as well as firefox parent process, or just one of its content process.
 * It can be very similar to a Target. The key difference is the lifecycle of these two classes.
 * The descriptor is meant to be always alive and meaningful/usable until the end of the RDP connection.
 * Typically a Tab Descriptor will describe the tab and not the one document currently loaded in this tab,
 * while the Target, will describe this one document and a new Target may be created on each navigation.
 */
function DescriptorMixin(parentClass) {
  class Descriptor extends parentClass {
    constructor(client, targetFront, parentFront) {
      super(client, targetFront, parentFront);

      this._client = client;

      // Pass a true value in order to distinguish this event reception
      // from any manual destroy caused by the frontend
      this.on(
        "descriptor-destroyed",
        this.destroy.bind(this, { isServerDestroyEvent: true })
      );
    }

    get client() {
      return this._client;
    }

    async getCommands() {
      if (!this._commands) {
        this._commands = createCommandsDictionary(this);
      }
      return this._commands;
    }
  }
  return Descriptor;
}
exports.DescriptorMixin = DescriptorMixin;
