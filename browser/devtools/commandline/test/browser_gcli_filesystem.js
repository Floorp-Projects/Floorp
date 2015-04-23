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

// THIS FILE IS GENERATED FROM SOURCE IN THE GCLI PROJECT
// PLEASE TALK TO SOMEONE IN DEVELOPER TOOLS BEFORE EDITING IT

const exports = {};

function test() {
  helpers.runTestModule(exports, "browser_gcli_filesystem.js");
}

// var assert = require('../testharness/assert');
// var helpers = require('./helpers');
var filesystem = require('gcli/util/filesystem');

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
