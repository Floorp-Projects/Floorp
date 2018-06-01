/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

const {Cu} = require("chrome");
const {deviceSpec} = require("devtools/shared/specs/device");
const protocol = require("devtools/shared/protocol");
const defer = require("devtools/shared/defer");

const DeviceFront = protocol.FrontClassWithSpec(deviceSpec, {
  initialize: function(client, form) {
    protocol.Front.prototype.initialize.call(this, client);
    this.actorID = form.deviceActor;
    this.manage(this);
  },

  screenshotToBlob: function() {
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
  },
});

const _knownDeviceFronts = new WeakMap();

/**
 * Retrieve the device front already created for the provided client, if available.
 */
exports.getKnownDeviceFront = function(client) {
  return _knownDeviceFronts.get(client);
};

/**
 * Only one DeviceFront is created for a given client, afterwards the instance is cached
 * and returned immediately.
 */
exports.getDeviceFront = function(client, form) {
  if (!form.deviceActor) {
    return null;
  }

  if (_knownDeviceFronts.has(client)) {
    return _knownDeviceFronts.get(client);
  }

  const front = new DeviceFront(client, form);
  _knownDeviceFronts.set(client, front);
  return front;
};
