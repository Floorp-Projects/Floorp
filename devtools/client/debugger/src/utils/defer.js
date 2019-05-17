/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

// @flow

type Deferred<T> = {
  promise: Promise<T>,
  resolve: (arg: T) => mixed,
  reject: (arg: mixed) => mixed,
};

export default function defer<T>(): Deferred<T> {
  let resolve = () => {};
  let reject = () => {};
  const promise = new Promise((_res, _rej) => {
    resolve = _res;
    reject = _rej;
  });

  return { resolve, reject, promise };
}
