/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

"use strict";

function ObjectFront(grip, overrides) {
  return {
    grip,
    enumEntries() {
      return Promise.resolve(
        this.getIterator({
          ownProperties: {},
        })
      );
    },
    enumProperties(options) {
      return Promise.resolve(
        this.getIterator({
          ownProperties: {},
        })
      );
    },
    enumSymbols() {
      return Promise.resolve(
        this.getIterator({
          ownSymbols: [],
        })
      );
    },
    enumPrivateProperties() {
      return Promise.resolve(
        this.getIterator({
          privateProperties: [],
        })
      );
    },
    getPrototype() {
      return Promise.resolve({
        prototype: {},
      });
    },
    // Declared here so we can override it.
    getIterator(res) {
      return {
        slice(start, count) {
          return Promise.resolve(res);
        },
      };
    },
    ...overrides,
  };
}

module.exports = ObjectFront;
