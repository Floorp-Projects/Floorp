/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

const { Cu } = require("chrome");
const memory = require("sdk/test/memory");
const { add, remove, has, clear, iterator } = require("sdk/lang/weak-set");
const { setInterval, clearInterval } = require("sdk/timers");

function gc(assert) {
  let wait = 1;
  let interval = setInterval(function() {
    assert.pass("waited " + (wait++ * 0.250) + "secs for gc()..");
  }, 250);

  return memory.gc().then(() => {
    assert.pass("gc completed!");
    clearInterval(interval);
  });
}

exports['test add/remove/iterate/clear item'] = function*(assert) {
  let addItems = {};
  let removeItems = {};
  let iterateItems = {};
  let clearItems = {};
  let nonReferencedItems = {};

  let item = {};
  let addedItems = [{}, {}];

  assert.pass("adding things to items");
  add(addItems, item);
  add(removeItems, item);
  add(iterateItems, addedItems[0]);
  add(iterateItems, addedItems[1]);
  add(iterateItems, addedItems[0]); // weak set shouldn't add this twice
  add(clearItems, addedItems[0]);
  add(clearItems, addedItems[1]);
  add(nonReferencedItems, {});

  assert.pass("removing things from removeItems");
  remove(removeItems, item);

  assert.pass("clear things from clearItems");
  clear(clearItems);

  assert.pass("starting gc..");
  yield gc(assert);
  let count = 0;

  assert.equal(has(addItems, item), true, 'the addItems is in the weak set');
  assert.equal(has(removeItems, item), false, 'the removeItems is not in weak set');

  assert.pass("iterating iterateItems..");
  for (let item of iterator(iterateItems)) {
    assert.equal(item, addedItems[count], "item in the expected order");
    count++;
  }

  assert.equal(count, 2, 'items in the expected number');

  assert.pass("iterating clearItems..");
  for (let item of iterator(clearItems)) {
    assert.fail("the loop should not be executed");
    count++
  }

  for (let item of iterator(nonReferencedItems)) {
    assert.fail("the loop should not be executed");
    count++
  }

  assert.equal(count, 2, 'items in the expected number');
};

exports['test adding non object or null item'] = function(assert) {
  let items = {};

  assert.throws(() => {
    add(items, 'foo');
  },
  /^\w+ is not a non-null object/,
  'only non-null object are allowed');

  assert.throws(() => {
    add(items, 0);
  },
  /^\w+ is not a non-null object/,
  'only non-null object are allowed');

  assert.throws(() => {
    add(items, undefined);
  },
  /^\w+ is not a non-null object/,
  'only non-null object are allowed');

  assert.throws(() => {
    add(items, null);
  },
  /^\w+ is not a non-null object/,
  'only non-null object are allowed');

  assert.throws(() => {
    add(items, true);
  },
  /^\w+ is not a non-null object/,
  'only non-null object are allowed');
};

exports['test adding to non object or null item'] = function(assert) {
  let item = {};

  assert.throws(() => {
    add('foo', item);
  },
  /^\w+ is not a non-null object/,
  'only non-null object are allowed');

  assert.throws(() => {
    add(0, item);
  },
  /^\w+ is not a non-null object/,
  'only non-null object are allowed');

  assert.throws(() => {
    add(undefined, item);
  },
  /^\w+ is not a non-null object/,
  'only non-null object are allowed');

  assert.throws(() => {
    add(null, item);
  },
  /^\w+ is not a non-null object/,
  'only non-null object are allowed');

  assert.throws(() => {
    add(true, item);
  },
  /^\w+ is not a non-null object/,
  'only non-null object are allowed');
};

require("sdk/test").run(exports);
