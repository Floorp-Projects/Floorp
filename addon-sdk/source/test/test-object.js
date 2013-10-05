/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
'use strict';

const { merge, extend, has, each } = require('sdk/util/object');

let o = {
  'paper': 0,
  'rock': 1,
  'scissors': 2
};

//exports.testMerge = function(assert) {}
//exports.testExtend = function(assert) {}

exports.testHas = function(assert) {
  assert.equal(has(o, 'paper'), true, 'has correctly finds key');
  assert.equal(has(o, 'rock'), true, 'has correctly finds key');
  assert.equal(has(o, 'scissors'), true, 'has correctly finds key');
  assert.equal(has(o, 'nope'), false, 'has correctly does not find key');
  assert.equal(has(o, '__proto__'), false, 'has correctly does not find key');
  assert.equal(has(o, 'isPrototypeOf'), false, 'has correctly does not find key');
};

exports.testEach = function(assert) {
  var keys = new Set();
  each(o, function (value, key, object) {
    keys.add(key);
    assert.equal(o[key], value, 'Key and value pairs passed in');
    assert.equal(o, object, 'Object passed in');
  });
  assert.equal(keys.size, 3, 'All keys have been iterated upon');
};

require('sdk/test').run(exports);
