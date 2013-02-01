/* -*- Mode: Java; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

module.metadata = {
  "stability": "experimental"
};

exports.Collection = Collection;

/**
 * Adds a collection property to the given object.  Setting the property to a
 * scalar value empties the collection and adds the value.  Setting it to an
 * array empties the collection and adds all the items in the array.
 *
 * @param obj
 *        The property will be defined on this object.
 * @param propName
 *        The name of the property.
 * @param array
 *        If given, this will be used as the collection's backing array.
 */
exports.addCollectionProperty = function addCollProperty(obj, propName, array) {
  array = array || [];
  let publicIface = new Collection(array);

  Object.defineProperty(obj, propName, {
    configurable: true,
    enumerable: true,

    set: function set(itemOrItems) {
      array.splice(0, array.length);
      publicIface.add(itemOrItems);
    },

    get: function get() {
      return publicIface;
    }
  });
};

/**
 * A collection is ordered, like an array, but its items are unique, like a set.
 *
 * @param array
 *        The collection is backed by an array.  If this is given, it will be
 *        used as the backing array.  This way the caller can fully control the
 *        collection.  Otherwise a new empty array will be used, and no one but
 *        the collection will have access to it.
 */
function Collection(array) {
  array = array || [];

  /**
   * Provides iteration over the collection.  Items are yielded in the order
   * they were added.
   */
  this.__iterator__ = function Collection___iterator__() {
    let items = array.slice();
    for (let i = 0; i < items.length; i++)
      yield items[i];
  };

  /**
   * The number of items in the collection.
   */
  this.__defineGetter__("length", function Collection_get_length() {
    return array.length;
  });

  /**
   * Adds a single item or an array of items to the collection.  Any items
   * already contained in the collection are ignored.
   *
   * @param  itemOrItems
   *         An item or array of items.
   * @return The collection.
   */
  this.add = function Collection_add(itemOrItems) {
    let items = toArray(itemOrItems);
    for (let i = 0; i < items.length; i++) {
      let item = items[i];
      if (array.indexOf(item) < 0)
        array.push(item);
    }
    return this;
  };

  /**
   * Removes a single item or an array of items from the collection.  Any items
   * not contained in the collection are ignored.
   *
   * @param  itemOrItems
   *         An item or array of items.
   * @return The collection.
   */
  this.remove = function Collection_remove(itemOrItems) {
    let items = toArray(itemOrItems);
    for (let i = 0; i < items.length; i++) {
      let idx = array.indexOf(items[i]);
      if (idx >= 0)
        array.splice(idx, 1);
    }
    return this;
  };
};

function toArray(itemOrItems) {
  let isArr = itemOrItems &&
              itemOrItems.constructor &&
              itemOrItems.constructor.name === "Array";
  return isArr ? itemOrItems : [itemOrItems];
}
