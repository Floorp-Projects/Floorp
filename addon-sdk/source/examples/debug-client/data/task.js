/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

(function(exports) {
"use strict";

const spawn = (task, ...args) => {
  return new Promise((resolve, reject) => {
    try {
      const routine = task(...args);
      const raise = error => routine.throw(error);
      const step = data => {
        const { done, value } = routine.next(data);
        if (done)
          resolve(value);
        else
          Promise.resolve(value).then(step, raise);
      }
      step();
    } catch(error) {
      reject(error);
    }
  });
}
exports.spawn = spawn;

})(Task = {});
