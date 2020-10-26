/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { deviceSpec } = require("devtools/shared/specs/device");
const {
  FrontClassWithSpec,
  registerFront,
} = require("devtools/shared/protocol");

class DeviceFront extends FrontClassWithSpec(deviceSpec) {
  constructor(client, targetFront, parentFront) {
    super(client, targetFront, parentFront);

    // Attribute name from which to retrieve the actorID out of the target actor's form
    this.formAttributeName = "deviceActor";
  }

  /**
   * Handle backward compatibility for getDescription.
   * Can be removed on Firefox 70 reaches the release channel.
   */
  async getDescription() {
    const description = await super.getDescription();

    // Backward compatibility when connecting for Firefox 69 or older.
    if (typeof description.canDebugServiceWorkers === "undefined") {
      description.canDebugServiceWorkers = !description.isMultiE10s;
    }

    return description;
  }
}

exports.DeviceFront = DeviceFront;
registerFront(DeviceFront);
