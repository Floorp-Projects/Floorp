/* -*- Mode: Java; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const collection = require("sdk/util/collection");

exports.testAddRemove = function (test) {
  let coll = new collection.Collection();
  compare(test, coll, []);
  addRemove(test, coll, [], false);
};

exports.testAddRemoveBackingArray = function (test) {
  let items = ["foo"];
  let coll = new collection.Collection(items);
  compare(test, coll, items);
  addRemove(test, coll, items, true);

  items = ["foo", "bar"];
  coll = new collection.Collection(items);
  compare(test, coll, items);
  addRemove(test, coll, items, true);
};

exports.testProperty = function (test) {
  let obj = makeObjWithCollProp();
  compare(test, obj.coll, []);
  addRemove(test, obj.coll, [], false);

  // Test single-value set.
  let items = ["foo"];
  obj.coll = items[0];
  compare(test, obj.coll, items);
  addRemove(test, obj.coll, items, false);

  // Test array set.
  items = ["foo", "bar"];
  obj.coll = items;
  compare(test, obj.coll, items);
  addRemove(test, obj.coll, items, false);
};

exports.testPropertyBackingArray = function (test) {
  let items = ["foo"];
  let obj = makeObjWithCollProp(items);
  compare(test, obj.coll, items);
  addRemove(test, obj.coll, items, true);

  items = ["foo", "bar"];
  obj = makeObjWithCollProp(items);
  compare(test, obj.coll, items);
  addRemove(test, obj.coll, items, true);
};

// Adds some values to coll and then removes them.  initialItems is an array
// containing coll's initial items.  isBacking is true if initialItems is coll's
// backing array; the point is that updates to coll should affect initialItems
// if that's the case.
function addRemove(test, coll, initialItems, isBacking) {
  let items = isBacking ? initialItems : initialItems.slice(0);
  let numInitialItems = items.length;

  // Test add(val).
  let numInsertions = 5;
  for (let i = 0; i < numInsertions; i++) {
    compare(test, coll, items);
    coll.add(i);
    if (!isBacking)
      items.push(i);
  }
  compare(test, coll, items);

  // Add the items we just added to make sure duplicates aren't added.
  for (let i = 0; i < numInsertions; i++)
    coll.add(i);
  compare(test, coll, items);

  // Test remove(val).  Do a kind of shuffled remove.  Remove item 1, then
  // item 0, 3, 2, 5, 4, ...
  for (let i = 0; i < numInsertions; i++) {
    let val = i % 2 ? i - 1 :
              i === numInsertions - 1 ? i : i + 1;
    coll.remove(val);
    if (!isBacking)
      items.splice(items.indexOf(val), 1);
    compare(test, coll, items);
  }
  test.assertEqual(coll.length, numInitialItems,
                   "All inserted items should be removed");

  // Remove the items we just removed.  coll should be unchanged.
  for (let i = 0; i < numInsertions; i++)
    coll.remove(i);
  compare(test, coll, items);

  // Test add and remove([val1, val2]).
  let newItems = [0, 1];
  coll.add(newItems);
  compare(test, coll, isBacking ? items : items.concat(newItems));
  coll.remove(newItems);
  compare(test, coll, items);
  test.assertEqual(coll.length, numInitialItems,
                   "All inserted items should be removed");
}

// Asserts that the items in coll are the items of array.
function compare(test, coll, array) {
  test.assertEqual(coll.length, array.length,
                   "Collection length should be correct");
  let numItems = 0;
  for (let item in coll) {
    test.assertEqual(item, array[numItems], "Items should be equal");
    numItems++;
  }
  test.assertEqual(numItems, array.length,
                   "Number of items in iteration should be correct");
}

// Returns a new object with a collection property named "coll".  backingArray,
// if defined, will create the collection with that backing array.
function makeObjWithCollProp(backingArray) {
  let obj = {};
  collection.addCollectionProperty(obj, "coll", backingArray);
  return obj;
}
