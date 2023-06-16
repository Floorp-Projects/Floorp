/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const {
  FrontClassWithSpec,
  registerFront,
} = require("resource://devtools/shared/protocol.js");
const {
  privatePropertiesIteratorSpec,
} = require("resource://devtools/shared/specs/private-properties-iterator.js");
const {
  getAdHocFrontOrPrimitiveGrip,
} = require("resource://devtools/client/fronts/object.js");

class PrivatePropertiesIteratorFront extends FrontClassWithSpec(
  privatePropertiesIteratorSpec
) {
  form(data) {
    this.actorID = data.actor;
    this.count = data.count;
  }

  async slice(start, count) {
    const result = await super.slice({ start, count });
    return this._onResult(result);
  }

  async all() {
    const result = await super.all();
    return this._onResult(result);
  }

  _onResult(result) {
    if (!result.privateProperties) {
      return result;
    }

    // The result packet can have multiple properties that hold grips which we may need
    // to turn into fronts.
    const gripKeys = ["value", "getterValue", "get", "set"];

    result.privateProperties.forEach((item, i) => {
      if (item?.descriptor) {
        for (const gripKey of gripKeys) {
          if (item.descriptor.hasOwnProperty(gripKey)) {
            result.privateProperties[i].descriptor[gripKey] =
              getAdHocFrontOrPrimitiveGrip(item.descriptor[gripKey], this);
          }
        }
      }
    });
    return result;
  }
}

exports.PrivatePropertiesIteratorFront = PrivatePropertiesIteratorFront;
registerFront(PrivatePropertiesIteratorFront);
