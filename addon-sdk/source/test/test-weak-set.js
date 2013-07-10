/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

'use strict';

const { Cu } = require('chrome');
const { Loader } = require('sdk/test/loader');
const { defer } = require('sdk/core/promise');

function gc() {
  let { promise, resolve } = defer();

  Cu.schedulePreciseGC(resolve);

  return promise;
};

exports['test adding item'] = function(assert, done) {
  let loader = Loader(module);
  let { add, remove, has, clear, iterator } = loader.require('sdk/lang/weak-set');

  let items = {};
  let item = {};

  add(items, item);

  gc().
    then(() => {
      assert.ok(has(items, item),
        'the item is in the weak set');
    }).
    then(loader.unload).
    then(done, assert.fail);
};

exports['test remove item'] = function(assert, done) {
  let loader = Loader(module);
  let { add, remove, has, clear, iterator } = loader.require('sdk/lang/weak-set');

  let items = {};
  let item = {};

  add(items, item);

  remove(items, item);

  gc().
    then(() => {
      assert.ok(!has(items, item),
        'the item is not in weak set');
    }).
    then(loader.unload).
    then(done, assert.fail);
};

exports['test iterate'] = function(assert, done) {
  let loader = Loader(module);
  let { add, remove, has, clear, iterator } = loader.require('sdk/lang/weak-set');

  let items = {};
  let addedItems = [{}, {}];

  add(items, addedItems[0]);
  add(items, addedItems[1]);
  add(items, addedItems[0]); // weak set shouldn't add this twice

  gc().
    then(() => {
      let count = 0;

      for (let item of iterator(items)) {
        assert.equal(item, addedItems[count],
          'item in the expected order');

        count++;
      }

      assert.equal(count, 2,
        'items in the expected number');
    }).
    then(loader.unload).
    then(done, assert.fail);
};

exports['test clear'] = function(assert, done) {
  let loader = Loader(module);
  let { add, remove, has, clear, iterator } = loader.require('sdk/lang/weak-set');

  let items = {};
  let addedItems = [{}, {}];

  add(items, addedItems[0]);
  add(items, addedItems[1]);

  clear(items)

  gc().
    then(() => {
      let count = 0;

      for (let item of iterator(items))
        assert.fail('the loop should not be executed');

      assert.equal(count, 0,
        'no items in the weak set');
    }).
    then(loader.unload).
    then(done, assert.fail);
};

exports['test adding item without reference'] = function(assert, done) {
  let loader = Loader(module);
  let { add, remove, has, clear, iterator } = loader.require('sdk/lang/weak-set');

  let items = {};

  add(items, {});

  gc().
    then(() => {
      let count = 0;

      for (let item of iterator(items))
        assert.fail('the loop should not be executed');

      assert.equal(count, 0,
        'no items in the weak set');
    }).
    then(loader.unload).
    then(done, assert.fail);
};

exports['test adding non object or null item'] = function(assert) {
  let loader = Loader(module);
  let { add, remove, has, clear, iterator } = loader.require('sdk/lang/weak-set');

  let items = {};

  assert.throws(() => {
    add(items, 'foo');
  },
  /^value is not a non-null object/,
  'only non-null object are allowed');

  assert.throws(() => {
    add(items, 0);
  },
  /^value is not a non-null object/,
  'only non-null object are allowed');

  assert.throws(() => {
    add(items, undefined);
  },
  /^value is not a non-null object/,
  'only non-null object are allowed');

  assert.throws(() => {
    add(items, null);
  },
  /^value is not a non-null object/,
  'only non-null object are allowed');

  assert.throws(() => {
    add(items, true);
  },
  /^value is not a non-null object/,
  'only non-null object are allowed');
};

exports['test adding to non object or null item'] = function(assert) {
  let loader = Loader(module);
  let { add, remove, has, clear, iterator } = loader.require('sdk/lang/weak-set');

  let item = {};

  assert.throws(() => {
    add('foo', item);
  },
  /^value is not a non-null object/,
  'only non-null object are allowed');

  assert.throws(() => {
    add(0, item);
  },
  /^value is not a non-null object/,
  'only non-null object are allowed');

  assert.throws(() => {
    add(undefined, item);
  },
  /^value is not a non-null object/,
  'only non-null object are allowed');

  assert.throws(() => {
    add(null, item);
  },
  /^value is not a non-null object/,
  'only non-null object are allowed');

  assert.throws(() => {
    add(true, item);
  },
  /^value is not a non-null object/,
  'only non-null object are allowed');
};

require('test').run(exports);
