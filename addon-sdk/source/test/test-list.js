/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
'use strict';

const { List, addListItem, removeListItem } = require('sdk/util/list');
const { Class } = require('sdk/core/heritage');

exports.testList = function(test) {
  let list = List();
  addListItem(list, 1);

  for (let key in list) {
    test.assertEqual(key, 0, 'key is correct');
    test.assertEqual(list[key], 1, 'value is correct');
  }

  let count = 0;
  for each (let ele in list) {
    test.assertEqual(ele, 1, 'ele is correct');
    test.assertEqual(++count, 1, 'count is correct');
  }

  count = 0;
  for (let ele of list) {
    test.assertEqual(ele, 1, 'ele is correct');
    test.assertEqual(++count, 1, 'count is correct');
  }

  removeListItem(list, 1);
  test.assertEqual(list.length, 0, 'remove worked');
};

exports.testImplementsList = function(test) {
  let List2 = Class({
    implements: [List],
    initialize: function() {
      List.prototype.initialize.apply(this, [0, 1, 2]);
    }
  });
  let list2 = List2();
  let count = 0;

  for each (let ele in list2) {
    test.assertEqual(ele, count++, 'ele is correct');
  }

  count = 0;
  for (let ele of list2) {
    test.assertEqual(ele, count++, 'ele is correct');
  }

  addListItem(list2, 3);
  test.assertEqual(list2.length, 4, '3 was added');
  test.assertEqual(list2[list2.length-1], 3, '3 was added');
}
