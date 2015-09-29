/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
'use strict';

module.metadata = {
  "stability": "experimental"
};

const { Class } = require('../core/heritage');
const listNS = require('../core/namespace').ns();

const listOptions = {
  /**
   * List constructor can take any number of element to populate itself.
   * @params {Object|String|Number} element
   * @example
   *    List(1,2,3).length == 3 // true
   */
  initialize: function List() {
    listNS(this).keyValueMap = [];

    for (let i = 0, ii = arguments.length; i < ii; i++)
      addListItem(this, arguments[i]);
  },
  /**
   * Number of elements in this list.
   * @type {Number}
   */
  get length() {
    return listNS(this).keyValueMap.length;
  },
   /**
    * Returns a string representing this list.
    * @returns {String}
    */
  toString: function toString() {
    return 'List(' + listNS(this).keyValueMap + ')';
  },
  /**
   * Custom iterator providing `List`s enumeration behavior.
   * We cant reuse `_iterator` that is defined by `Iterable` since it provides
   * iteration in an arbitrary order.
   * @see https://developer.mozilla.org/en/JavaScript/Reference/Statements/for...in
   * @param {Boolean} onKeys
   */
  __iterator__: function __iterator__(onKeys, onKeyValue) {
    let array = listNS(this).keyValueMap.slice(0),
                i = -1;
    for (let element of array)
      yield onKeyValue ? [++i, element] : onKeys ? ++i : element;
  },
};
listOptions[Symbol.iterator] = function iterator() {
    return listNS(this).keyValueMap.slice(0)[Symbol.iterator]();
};
const List = Class(listOptions);
exports.List = List;

function addListItem(that, value) {
  let list = listNS(that).keyValueMap,
      index = list.indexOf(value);

  if (-1 === index) {
    try {
      that[that.length] = value;
    }
    catch (e) {}
    list.push(value);
  }
}
exports.addListItem = addListItem;

function removeListItem(that, element) {
  let list = listNS(that).keyValueMap,
      index = list.indexOf(element);

  if (0 <= index) {
    list.splice(index, 1);
    try {
      for (let length = list.length; index < length; index++)
        that[index] = list[index];
      that[list.length] = undefined;
    }
    catch(e){}
  }
}
exports.removeListItem = removeListItem;

exports.listNS = listNS;
