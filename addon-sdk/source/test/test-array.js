/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
'use strict'

const array = require('sdk/util/array');

exports.testHas = function(test) {
  var testAry = [1, 2, 3];
  test.assertEqual(array.has([1, 2, 3], 1), true);
  test.assertEqual(testAry.length, 3);
  test.assertEqual(testAry[0], 1);
  test.assertEqual(testAry[1], 2);
  test.assertEqual(testAry[2], 3);
  test.assertEqual(array.has(testAry, 2), true);
  test.assertEqual(array.has(testAry, 3), true);
  test.assertEqual(array.has(testAry, 4), false);
  test.assertEqual(array.has(testAry, '1'), false);
};
exports.testHasAny = function(test) {
  var testAry = [1, 2, 3];
  test.assertEqual(array.hasAny([1, 2, 3], [1]), true);
  test.assertEqual(array.hasAny([1, 2, 3], [1, 5]), true);
  test.assertEqual(array.hasAny([1, 2, 3], [5, 1]), true);
  test.assertEqual(array.hasAny([1, 2, 3], [5, 2]), true);
  test.assertEqual(array.hasAny([1, 2, 3], [5, 3]), true);
  test.assertEqual(array.hasAny([1, 2, 3], [5, 4]), false);
  test.assertEqual(testAry.length, 3);
  test.assertEqual(testAry[0], 1);
  test.assertEqual(testAry[1], 2);
  test.assertEqual(testAry[2], 3);
  test.assertEqual(array.hasAny(testAry, [2]), true);
  test.assertEqual(array.hasAny(testAry, [3]), true);
  test.assertEqual(array.hasAny(testAry, [4]), false);
  test.assertEqual(array.hasAny(testAry), false);
  test.assertEqual(array.hasAny(testAry, '1'), false);
};

exports.testAdd = function(test) {
  var testAry = [1];
  test.assertEqual(array.add(testAry, 1), false);
  test.assertEqual(testAry.length, 1);
  test.assertEqual(testAry[0], 1);
  test.assertEqual(array.add(testAry, 2), true);
  test.assertEqual(testAry.length, 2);
  test.assertEqual(testAry[0], 1);
  test.assertEqual(testAry[1], 2);
};

exports.testRemove = function(test) {
  var testAry = [1, 2];
  test.assertEqual(array.remove(testAry, 3), false);
  test.assertEqual(testAry.length, 2);
  test.assertEqual(testAry[0], 1);
  test.assertEqual(testAry[1], 2);
  test.assertEqual(array.remove(testAry, 2), true);
  test.assertEqual(testAry.length, 1);
  test.assertEqual(testAry[0], 1);
};

exports.testFlatten = function(test) {
  test.assertEqual(array.flatten([1, 2, 3]).length, 3);
  test.assertEqual(array.flatten([1, [2, 3]]).length, 3);
  test.assertEqual(array.flatten([1, [2, [3]]]).length, 3);
  test.assertEqual(array.flatten([[1], [[2, [3]]]]).length, 3);
};

exports.testUnique = function(test) {
  var Class = function () {};
  var A = {};
  var B = new Class();
  var C = [ 1, 2, 3 ];
  var D = {};
  var E = new Class();

  compareArray(array.unique([1,2,3,1,2]), [1,2,3]);
  compareArray(array.unique([1,1,1,4,9,5,5]), [1,4,9,5]);
  compareArray(array.unique([A, A, A, B, B, D]), [A,B,D]);
  compareArray(array.unique([A, D, A, E, E, D, A, A, C]), [A, D, E, C])

  function compareArray (a, b) {
    test.assertEqual(a.length, b.length);
    for (let i = 0; i < a.length; i++) {
      test.assertEqual(a[i], b[i]);
    }
  }
};

exports.testFind = function(test) {
  let isOdd = (x) => x % 2;
  test.assertEqual(array.find([2, 4, 5, 7, 8, 9], isOdd), 5);
  test.assertEqual(array.find([2, 4, 6, 8], isOdd), undefined);
  test.assertEqual(array.find([2, 4, 6, 8], isOdd, null), null);
};

