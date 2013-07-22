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

// define(function(require, exports, module) {

// <INJECTED SOURCE:START>

// THIS FILE IS GENERATED FROM SOURCE IN THE GCLI PROJECT
// DO NOT EDIT IT DIRECTLY

var exports = {};

const TEST_URI = "data:text/html;charset=utf-8,<p id='gcli-input'>gcli-testFilesystem.js</p>";

function test() {
  helpers.addTabWithToolbar(TEST_URI, function(options) {
    return helpers.runTests(options, exports);
  }).then(finish);
}

// <INJECTED SOURCE:END>

'use strict';

// var assert = require('test/assert');
// var helpers = require('gclitest/helpers');
var filesystem = require('util/filesystem');

exports.testSplit = function(options) {
  if (!options.isNode) {
    return;
  }

  helpers.arrayIs(filesystem.split('', '/'),
                  [ '.' ],
                  'split <blank>');

  helpers.arrayIs(filesystem.split('a', '/'),
                  [ 'a' ],
                  'split a');

  helpers.arrayIs(filesystem.split('a/b/c', '/'),
                  [ 'a', 'b', 'c' ],
                  'split a/b/c');

  helpers.arrayIs(filesystem.split('/a/b/c/', '/'),
                  [ 'a', 'b', 'c' ],
                  'split a/b/c');

  helpers.arrayIs(filesystem.split('/a/b///c/', '/'),
                  [ 'a', 'b', 'c' ],
                  'split a/b/c');
};

exports.testJoin = function(options) {
  if (!options.isNode) {
    return;
  }

  assert.is(filesystem.join('usr', 'local', 'bin'),
            'usr/local/bin',
            'join to usr/local/bin');
};

// });
