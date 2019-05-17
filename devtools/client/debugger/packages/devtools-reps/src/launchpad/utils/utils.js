/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

/**
 * Returns an object with a promise and its resolve and reject function,
 * so they can be called outside of the promise callback.
 *
 * @returns {{resolve: function, reject: function, promise: Promise}}
 */
function defer() {
  let resolve, reject;
  const promise = new Promise((res, rej) => {
    resolve = res;
    reject = rej;
  });

  return {
    resolve,
    reject,
    promise,
  };
}

/**
 * Takes a function and executes it on the next tick.
 *
 * @param function fn
 */
function executeSoon(fn) {
  setTimeout(fn, 0);
}

/**
 * Takes an object into another object,
 * filtered on its keys by the given predicate.
 *
 * @param object obj
 * @param function predicate
 * @returns object
 */
function filterByKey(obj, predicate) {
  return Object.keys(obj).reduce((res, key) => {
    if (predicate(key)) {
      return Object.assign(res, { [key]: obj[key] });
    }
    return res;
  }, {});
}

function generateKey() {
  return `${performance.now()}`;
}

module.exports = {
  defer,
  executeSoon,
  filterByKey,
  generateKey,
};
