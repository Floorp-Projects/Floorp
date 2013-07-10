/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
'use strict';

const { defer, all } = require('sdk/core/promise');
const { setTimeout } = require('sdk/timers');
try {
const { TreeNode } = require('sdk/places/utils');
} catch(e) { unsupported(e); }

exports['test construct tree'] = function (assert) {
  let tree = TreeNode(1);
  tree.add([2, 3, 4]);
  tree.get(2).add([2.1, 2.2, 2.3]);
  let newTreeNode = TreeNode(4.3);
  newTreeNode.add([4.31, 4.32]);
  tree.get(4).add([4.1, 4.2, newTreeNode]);

  assert.equal(tree.get(2).value, 2, 'get returns node with correct value');
  assert.equal(tree.get(2.3).value, 2.3, 'get returns node with correct value');
  assert.equal(tree.get(4.32).value, 4.32, 'get returns node even if created from nested node');
  assert.equal(tree.get(4).children.length, 3, 'nodes have correct children length');
  assert.equal(tree.get(3).children.length, 0, 'nodes have correct children length');

  assert.equal(tree.get(4).get(4.32).value, 4.32, 'node.get descends from itself');
  assert.equal(tree.get(4).get(2), null, 'node.get descends from itself fails if not descendant');
};

exports['test walk'] = function (assert) {
  let resultsAll = [];
  let tree = TreeNode(1);
  tree.add([2, 3, 4]);
  tree.get(2).add([2.1, 2.2]);

  tree.walk(function (node) {
    resultsAll.push(node.value);
  });

  [1, 2, 2.1, 2.2, 3, 4].forEach(function (num) {
    assert.ok(~resultsAll.indexOf(num), 'function applied to each node from root');
  });

  let resultsNode = [];
  tree.get(2).walk(function (node) resultsNode.push(node.value));

  [2, 2.1, 2.2].forEach(function (num) {
    assert.ok(~resultsNode.indexOf(num), 'function applied to each node from node');
  });
};

exports['test async walk'] = function (assert, done) {
  let resultsAll = [];
  let tree = TreeNode(1);
  tree.add([2, 3, 4]);
  tree.get(2).add([2.1, 2.2]);

  tree.walk(function (node) {
    let deferred = defer();
    setTimeout(function () {
      resultsAll.push(node.value);
      deferred.resolve(node.value);
    }, node.value === 2 ? 50 : 5);
    return deferred.promise;
  }).then(function () {
    [1, 2, 2.1, 2.2, 3, 4].forEach(function (num) {
      assert.ok(~resultsAll.indexOf(num), 'function applied to each node from root');
    });
    assert.ok(resultsAll.indexOf(2) < resultsAll.indexOf(2.1),
      'child should wait for parent to complete');
    assert.ok(resultsAll.indexOf(2) < resultsAll.indexOf(2.2),
      'child should wait for parent to complete');
    done();
  });
};

// If the module doesn't support the app we're being run in, require() will
// throw.  In that case, remove all tests above from exports, and add one dummy
// test that passes.
function unsupported (err) {
  if (!/^Unsupported Application/.test(err.message))
    throw err;

  module.exports = {
    "test Unsupported Application": function Unsupported (assert) {
      assert.pass(err.message);
    }
  };
}

require('test').run(exports);
