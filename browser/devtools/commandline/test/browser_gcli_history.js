/*
 * Copyright 2012, Mozilla Foundation and contributors
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

'use strict';
// <INJECTED SOURCE:START>

// THIS FILE IS GENERATED FROM SOURCE IN THE GCLI PROJECT
// DO NOT EDIT IT DIRECTLY

var exports = {};

var TEST_URI = "data:text/html;charset=utf-8,<p id='gcli-input'>gcli-testHistory.js</p>";

function test() {
  return Task.spawn(function() {
    let options = yield helpers.openTab(TEST_URI);
    yield helpers.openToolbar(options);
    gcli.addItems(mockCommands.items);

    yield helpers.runTests(options, exports);

    gcli.removeItems(mockCommands.items);
    yield helpers.closeToolbar(options);
    yield helpers.closeTab(options);
  }).then(finish, helpers.handleError);
}

// <INJECTED SOURCE:END>

// var assert = require('../testharness/assert');
var History = require('gcli/ui/history').History;

exports.testSimpleHistory = function (options) {
  var history = new History({});
  history.add('foo');
  history.add('bar');
  assert.is(history.backward(), 'bar');
  assert.is(history.backward(), 'foo');

  // Adding to the history again moves us back to the start of the history.
  history.add('quux');
  assert.is(history.backward(), 'quux');
  assert.is(history.backward(), 'bar');
  assert.is(history.backward(), 'foo');
};

exports.testBackwardsPastIndex = function (options) {
  var history = new History({});
  history.add('foo');
  history.add('bar');
  assert.is(history.backward(), 'bar');
  assert.is(history.backward(), 'foo');

  // Moving backwards past recorded history just keeps giving you the last
  // item.
  assert.is(history.backward(), 'foo');
};

exports.testForwardsPastIndex = function (options) {
  var history = new History({});
  history.add('foo');
  history.add('bar');
  assert.is(history.backward(), 'bar');
  assert.is(history.backward(), 'foo');

  // Going forward through the history again.
  assert.is(history.forward(), 'bar');

  // 'Present' time.
  assert.is(history.forward(), '');

  // Going to the 'future' just keeps giving us the empty string.
  assert.is(history.forward(), '');
};
