/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

'use strict';

module.metadata = {
  'stability': 'unstable'
};

const { Class } = require('../core/heritage');
const { method } = require('../lang/functional');
const { defer, promised, all } = require('../core/promise');

/*
 * TreeNodes are used to construct dependency trees
 * for BookmarkItems
 */
let TreeNode = Class({
  initialize: function (value) {
    this.value = value;
    this.children = [];
  },
  add: function (values) {
    [].concat(values).forEach(value => {
      this.children.push(value instanceof TreeNode ? value : TreeNode(value));
    });
  },
  get length () {
    let count = 0;
    this.walk(() => count++);
    // Do not count the current node
    return --count;
  },
  get: method(get),
  walk: method(walk),
  toString: function () '[object TreeNode]'
});
exports.TreeNode = TreeNode;

/*
 * Descends down from `node` applying `fn` to each in order.
 * Can be asynchronous if `fn` returns a promise. `fn` is passed 
 * one argument, the current node, `curr`
 */
function walk (curr, fn) {
  return promised(fn)(curr).then(val => {
    return all(curr.children.map(child => walk(child, fn)));
  });
} 

/*
 * Descends from the TreeNode `node`, returning
 * the node with value `value` if found or `null`
 * otherwise
 */
function get (node, value) {
  if (node.value === value) return node;
  for (let child of node.children) {
    let found = get(child, value);
    if (found) return found;
  }
  return null;
}
