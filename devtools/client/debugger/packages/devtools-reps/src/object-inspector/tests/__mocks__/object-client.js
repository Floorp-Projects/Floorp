/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

function ObjectClient(grip, overrides) {
  return {
    grip,
    enumEntries: function() {
      return Promise.resolve({
        iterator: this.getIterator({
          ownProperties: {},
        }),
      });
    },
    enumProperties: function(options) {
      return Promise.resolve({
        iterator: this.getIterator({
          ownProperties: {},
        }),
      });
    },
    enumSymbols: function() {
      return Promise.resolve({
        iterator: this.getIterator({
          ownSymbols: [],
        }),
      });
    },
    getPrototype: function() {
      return Promise.resolve({
        prototype: {},
      });
    },
    // Declared here so we can override it.
    getIterator(res) {
      return {
        slice: function(start, count) {
          return Promise.resolve(res);
        },
      };
    },
    ...overrides,
  };
}

module.exports = ObjectClient;
