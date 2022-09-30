/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const {
  FrontClassWithSpec,
  registerFront,
} = require("resource://devtools/shared/protocol.js");
const {
  symbolIteratorSpec,
} = require("resource://devtools/shared/specs/symbol-iterator.js");
const {
  getAdHocFrontOrPrimitiveGrip,
} = require("resource://devtools/client/fronts/object.js");

/**
 * A SymbolIteratorFront is used as a front end for the SymbolIterator that is
 * created on the server, hiding implementation details.
 */
class SymbolIteratorFront extends FrontClassWithSpec(symbolIteratorSpec) {
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
    if (!result.ownSymbols) {
      return result;
    }

    result.ownSymbols.forEach((item, i) => {
      if (item?.descriptor) {
        result.ownSymbols[i].descriptor.value = getAdHocFrontOrPrimitiveGrip(
          item.descriptor.value,
          this
        );
      }
    });
    return result;
  }
}

exports.SymbolIteratorFront = SymbolIteratorFront;
registerFront(SymbolIteratorFront);
