/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
'use strict';

module.metadata = {
  'stability': 'experimental'
};

const method = require('../../method/core');
const { uuid } = require('../util/uuid');

// NOTE: use lang/functional memoize when it is updated to use WeakMap
function memoize(f) {
  const memo = new WeakMap();

  return function memoizer(o) {
    let key = o;
    if (!memo.has(key))
      memo.set(key, f.apply(this, arguments));
    return memo.get(key);
  };
}

let identify = method('identify');
identify.define(Object, memoize(function() { return uuid(); }));
exports.identify = identify;
