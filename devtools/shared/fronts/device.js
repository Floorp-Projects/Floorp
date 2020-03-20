/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { Cu } = require("chrome");
const { deviceSpec } = require("devtools/shared/specs/device");
const {
  FrontClassWithSpec,
  registerFront,
} = require("devtools/shared/protocol");
const defer = require("devtools/shared/defer");

class DeviceFront extends FrontClassWithSpec(deviceSpec) {
  constructor(client, targetFront, parentFront) {
    super(client, targetFront, parentFront);

    // Attribute name from which to retrieve the actorID out of the target actor's form
    this.formAttributeName = "deviceActor";

    // Backward compatibility when connected to Firefox 69 or older.
    // Re-emit the "multi-e10s-updated" event as "can-debug-sw-updated".
    // Front events are all cleared via EventEmitter::clearEvents in the Front
    // base class destroy.
    this.on("multi-e10s-updated", (e, isMultiE10s) => {
      this.emit("can-debug-sw-updated", !isMultiE10s);
    });
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

  screenshotToBlob() {
    return this.screenshotToDataURL().then(longstr => {
      return longstr.string().then(dataURL => {
        const deferred = defer();
        longstr.release().catch(Cu.reportError);
        const req = new XMLHttpRequest();
        req.open("GET", dataURL, true);
        req.responseType = "blob";
        req.onload = () => {
          deferred.resolve(req.response);
        };
        req.onerror = () => {
          deferred.reject(req.status);
        };
        req.send();
        return deferred.promise;
      });
    });
  }
}

exports.DeviceFront = DeviceFront;
registerFront(DeviceFront);
