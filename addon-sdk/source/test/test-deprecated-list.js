/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

function assertList(test, array, list) {
  for (let i = 0, ii = array.length; i < ii; i < ii, i++) {
    test.assertEqual(
      array.length,
      list.length,
      'list must contain same amount of elements as array'
    );
    test.assertEqual(
      'List(' + array + ')',
      list + '',
      'toString must output array like result'
    );
    test.assert(
      i in list,
      'must contain element with index: ' + i
    );
    test.assertEqual(
      array[i],
      list[i],
      'element with index: ' + i + ' should match'
    );
  }
}

const { List } = require('sdk/deprecated/list');

exports['test:test for'] = function(test) {
  let fixture = List(3, 2, 1);

  test.assertEqual(3, fixture.length, 'length is 3');
  let i = 0;
  for (let key in fixture) {
    test.assertEqual(i++, key, 'key should match');
  }
};

exports['test:test for each'] = function(test) {
  let fixture = new List(3, 2, 1);

  test.assertEqual(3, fixture.length, 'length is 3');
  let i = 3;
  for each (let value in fixture) {
    test.assertEqual(i--, value, 'value should match');
  }
};

exports['test:test toString'] = function(test) {
  let fixture = List(3, 2, 1);

  test.assertEqual(
    'List(3,2,1)',
    fixture + '',
    'toString must output array like result'
  )
};

exports['test:test constructor with apply'] = function(test) {
  let array = ['a', 'b', 'c'];
  let fixture = List.apply(null, array);

  test.assertEqual(
    3,
    fixture.length,
    'should have applied arguments'
  );
};

exports['test:direct element access'] = function(test) {
  let array = [1, 'foo', 2, 'bar', {}, 'bar', function a() {}, test, 1];
  let fixture = List.apply(null, array);
  array.splice(5, 1);
  array.splice(7, 1);

  test.assertEqual(
    array.length,
    fixture.length,
    'list should omit duplicate elements'
  );

  test.assertEqual(
    'List(' + array + ')',
    fixture.toString(),
    'elements should not be rearranged'
  );

  for (let key in array) {
    test.assert(key in fixture,'should contain key for index:' + key);
    test.assertEqual(
      array[key],
      fixture[key],
      'values should match for: ' + key
    );
  }
};

exports['test:removing adding elements'] = function(test) {
  let array = [1, 'foo', 2, 'bar', {}, 'bar', function a() {}, test, 1];
  let fixture = List.compose({
    add: function() this._add.apply(this, arguments),
    remove: function() this._remove.apply(this, arguments),
    clear: function() this._clear()
  }).apply(null, array);
  array.splice(5, 1);
  array.splice(7, 1);

  assertList(test, array, fixture);

  array.splice(array.indexOf(2), 1);
  fixture.remove(2);
  assertList(test, array, fixture);

  array.splice(array.indexOf('foo'), 1);
  fixture.remove('foo');
  array.splice(array.indexOf(1), 1);
  fixture.remove(1);
  array.push('foo');
  fixture.add('foo');
  assertList(test, array, fixture);

  array.splice(0);
  fixture.clear(0);
  assertList(test, array, fixture);

  array.push(1, 'foo', 2, 'bar', 3);
  fixture.add(1);
  fixture.add('foo');
  fixture.add(2);
  fixture.add('bar');
  fixture.add(3);

  assertList(test, array, fixture);
};

exports['test: remove does not leave invalid numerical properties'] = function(test) {
  let fixture = List.compose({
    remove: function() this._remove.apply(this, arguments),
  }).apply(null, [1, 2, 3]);

    fixture.remove(1);
    test.assertEqual(fixture[fixture.length], undefined);
}

exports['test:add list item from Iterator'] = function(test) {
  let array = [1, 2, 3, 4], sum = 0, added = false;

  let fixture = List.compose({
    add: function() this._add.apply(this, arguments),
  }).apply(null, array);

  for each (let item in fixture) {
    sum += item;

    if (!added) {
      fixture.add(5);
      added = true;
    }
  }

  test.assertEqual(sum, 1 + 2 + 3 + 4);
};

exports['test:remove list item from Iterator'] = function(test) {
  let array = [1, 2, 3, 4], sum = 0;

  let fixture = List.compose({
    remove: function() this._remove.apply(this, arguments),
  }).apply(null, array);

  for each (let item in fixture) {
    sum += item;
    fixture.remove(item);
  }

  test.assertEqual(sum, 1 + 2 + 3 + 4);
};

exports['test:clear list from Iterator'] = function(test) {
  let array = [1, 2, 3, 4], sum = 0;

  let fixture = List.compose({
    clear: function() this._clear()
  }).apply(null, array);

  for each (let item in fixture) {
    sum += item;
    fixture.clear();
  }

  test.assertEqual(sum, 1 + 2 + 3 + 4);
};
