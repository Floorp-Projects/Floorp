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

var TEST_URI = "data:text/html;charset=utf-8,<p id='gcli-input'>gcli-testSplit.js</p>";

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

var cli = require('gcli/cli');

exports.testSplitSimple = function(options) {
  var args = cli.tokenize('s');
  options.requisition._split(args);
  assert.is(args.length, 0);
  assert.is(options.requisition.commandAssignment.arg.text, 's');
};

exports.testFlatCommand = function(options) {
  var args = cli.tokenize('tsv');
  options.requisition._split(args);
  assert.is(args.length, 0);
  assert.is(options.requisition.commandAssignment.value.name, 'tsv');

  args = cli.tokenize('tsv a b');
  options.requisition._split(args);
  assert.is(options.requisition.commandAssignment.value.name, 'tsv');
  assert.is(args.length, 2);
  assert.is(args[0].text, 'a');
  assert.is(args[1].text, 'b');
};

exports.testJavascript = function(options) {
  if (!options.requisition.system.commands.get('{')) {
    assert.log('Skipping testJavascript because { is not registered');
    return;
  }

  var args = cli.tokenize('{');
  options.requisition._split(args);
  assert.is(args.length, 1);
  assert.is(args[0].text, '');
  assert.is(options.requisition.commandAssignment.arg.text, '');
  assert.is(options.requisition.commandAssignment.value.name, '{');
};

// BUG 663081 - add tests for sub commands
