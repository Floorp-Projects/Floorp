/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
'use strict';

const { merge, extend, has, each } = require('sdk/util/object');

let o = {
  'paper': 0,
  'rock': 1,
  'scissors': 2
}

//exports.testMerge = function(test) {}
//exports.testExtend = function(test) {}

exports.testHas = function(test) {
  test.assertEqual(has(o, 'paper'), true, 'has correctly finds key');
  test.assertEqual(has(o, 'rock'), true, 'has correctly finds key');
  test.assertEqual(has(o, 'scissors'), true, 'has correctly finds key');
  test.assertEqual(has(o, 'nope'), false, 'has correctly does not find key');
  test.assertEqual(has(o, '__proto__'), false, 'has correctly does not find key');
  test.assertEqual(has(o, 'isPrototypeOf'), false, 'has correctly does not find key');
};

exports.testEach = function(test) {
  var count = 0;
  var keys = new Set();
  each(o, function (value, key, object) {
    keys.add(key);
    test.assertEqual(o[key], value, 'Key and value pairs passed in');
    test.assertEqual(o, object, 'Object passed in');
  });
  test.assertEqual(keys.size, 3, 'All keys have been iterated upon');
}
