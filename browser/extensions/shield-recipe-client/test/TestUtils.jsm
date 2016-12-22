/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

/* eslint-disable no-console */
this.EXPORTED_SYMBOLS = ["TestUtils"];

this.TestUtils = {
  promiseTest(test) {
    return function(assert, done) {
      test(assert)
      .catch(err => {
        console.error(err);
        assert.ok(false, err);
      })
      .then(() => done());
    };
  },
};
