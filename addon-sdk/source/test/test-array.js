/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
'use strict'

const array = require('sdk/util/array');

exports.testHas = function(assert) {
  var testAry = [1, 2, 3];
  assert.equal(array.has([1, 2, 3], 1), true);
  assert.equal(testAry.length, 3);
  assert.equal(testAry[0], 1);
  assert.equal(testAry[1], 2);
  assert.equal(testAry[2], 3);
  assert.equal(array.has(testAry, 2), true);
  assert.equal(array.has(testAry, 3), true);
  assert.equal(array.has(testAry, 4), false);
  assert.equal(array.has(testAry, '1'), false);
};
exports.testHasAny = function(assert) {
  var testAry = [1, 2, 3];
  assert.equal(array.hasAny([1, 2, 3], [1]), true);
  assert.equal(array.hasAny([1, 2, 3], [1, 5]), true);
  assert.equal(array.hasAny([1, 2, 3], [5, 1]), true);
  assert.equal(array.hasAny([1, 2, 3], [5, 2]), true);
  assert.equal(array.hasAny([1, 2, 3], [5, 3]), true);
  assert.equal(array.hasAny([1, 2, 3], [5, 4]), false);
  assert.equal(testAry.length, 3);
  assert.equal(testAry[0], 1);
  assert.equal(testAry[1], 2);
  assert.equal(testAry[2], 3);
  assert.equal(array.hasAny(testAry, [2]), true);
  assert.equal(array.hasAny(testAry, [3]), true);
  assert.equal(array.hasAny(testAry, [4]), false);
  assert.equal(array.hasAny(testAry), false);
  assert.equal(array.hasAny(testAry, '1'), false);
};

exports.testAdd = function(assert) {
  var testAry = [1];
  assert.equal(array.add(testAry, 1), false);
  assert.equal(testAry.length, 1);
  assert.equal(testAry[0], 1);
  assert.equal(array.add(testAry, 2), true);
  assert.equal(testAry.length, 2);
  assert.equal(testAry[0], 1);
  assert.equal(testAry[1], 2);
};

exports.testRemove = function(assert) {
  var testAry = [1, 2];
  assert.equal(array.remove(testAry, 3), false);
  assert.equal(testAry.length, 2);
  assert.equal(testAry[0], 1);
  assert.equal(testAry[1], 2);
  assert.equal(array.remove(testAry, 2), true);
  assert.equal(testAry.length, 1);
  assert.equal(testAry[0], 1);
};

exports.testFlatten = function(assert) {
  assert.equal(array.flatten([1, 2, 3]).length, 3);
  assert.equal(array.flatten([1, [2, 3]]).length, 3);
  assert.equal(array.flatten([1, [2, [3]]]).length, 3);
  assert.equal(array.flatten([[1], [[2, [3]]]]).length, 3);
};

exports.testUnique = function(assert) {
  var Class = function () {};
  var A = {};
  var B = new Class();
  var C = [ 1, 2, 3 ];
  var D = {};
  var E = new Class();

  assert.deepEqual(array.unique([1,2,3,1,2]), [1,2,3]);
  assert.deepEqual(array.unique([1,1,1,4,9,5,5]), [1,4,9,5]);
  assert.deepEqual(array.unique([A, A, A, B, B, D]), [A,B,D]);
  assert.deepEqual(array.unique([A, D, A, E, E, D, A, A, C]), [A, D, E, C])
};

exports.testUnion = function(assert) {
  var Class = function () {};
  var A = {};
  var B = new Class();
  var C = [ 1, 2, 3 ];
  var D = {};
  var E = new Class();

  assert.deepEqual(array.union([1, 2, 3],[7, 1, 2]), [1, 2, 3, 7]);
  assert.deepEqual(array.union([1, 1, 1, 4, 9, 5, 5], [10, 1, 5]), [1, 4, 9, 5, 10]);
  assert.deepEqual(array.union([A, B], [A, D]), [A, B, D]);
  assert.deepEqual(array.union([A, D], [A, E], [E, D, A], [A, C]), [A, D, E, C]);
};

exports.testFind = function(assert) {
  let isOdd = (x) => x % 2;
  assert.equal(array.find([2, 4, 5, 7, 8, 9], isOdd), 5);
  assert.equal(array.find([2, 4, 6, 8], isOdd), undefined);
  assert.equal(array.find([2, 4, 6, 8], isOdd, null), null);
};

require('test').run(exports);
