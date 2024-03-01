/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const mockedESM = {
  BinarySearch: {
    insertionIndexOf() {
      return 0;
    },
  },
};

module.exports = {
  import: () => ({}),
  addProfilerMarker: () => {},
  defineESModuleGetters: (lazy, dict) => {
    for (const key in dict) {
      lazy[key] = mockedESM[key];
    }
  },
  importESModule: () => ({}),
};
