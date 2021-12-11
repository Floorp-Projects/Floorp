/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const {
  FrontClassWithSpec,
  registerFront,
} = require("devtools/shared/protocol");
const {
  privatePropertiesIteratorSpec,
} = require("devtools/shared/specs/private-properties-iterator");
const {
  getAdHocFrontOrPrimitiveGrip,
} = require("devtools/client/fronts/object");

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

    result.privateProperties.forEach((item, i) => {
      if (item?.descriptor) {
        result.privateProperties[
          i
        ].descriptor.value = getAdHocFrontOrPrimitiveGrip(
          item.descriptor.value,
          this
        );
      }
    });
    return result;
  }
}

exports.PrivatePropertiesIteratorFront = PrivatePropertiesIteratorFront;
registerFront(PrivatePropertiesIteratorFront);
