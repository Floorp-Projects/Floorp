/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const {
  FrontClassWithSpec,
  registerFront,
} = require("resource://devtools/shared/protocol.js");
const {
  propertyIteratorSpec,
} = require("resource://devtools/shared/specs/property-iterator.js");
const {
  getAdHocFrontOrPrimitiveGrip,
} = require("resource://devtools/client/fronts/object.js");

/**
 * A PropertyIteratorFront provides a way to access to property names and
 * values of an object efficiently, slice by slice.
 * Note that the properties can be sorted in the backend,
 * this is controled while creating the PropertyIteratorFront
 * from ObjectFront.enumProperties.
 */
class PropertyIteratorFront extends FrontClassWithSpec(propertyIteratorSpec) {
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
    if (!result.ownProperties) {
      return result;
    }

    // The result packet can have multiple properties that hold grips which we may need
    // to turn into fronts.
    const gripKeys = ["value", "getterValue", "get", "set"];

    Object.entries(result.ownProperties).forEach(([key, descriptor]) => {
      if (descriptor) {
        for (const gripKey of gripKeys) {
          if (descriptor.hasOwnProperty(gripKey)) {
            result.ownProperties[key][gripKey] = getAdHocFrontOrPrimitiveGrip(
              descriptor[gripKey],
              this
            );
          }
        }
      }
    });
    return result;
  }
}

exports.PropertyIteratorFront = PropertyIteratorFront;
registerFront(PropertyIteratorFront);
