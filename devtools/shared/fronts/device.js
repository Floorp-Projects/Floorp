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
  constructor(client) {
    super(client);

    // Attribute name from which to retrieve the actorID out of the target actor's form
    this.formAttributeName = "deviceActor";
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
