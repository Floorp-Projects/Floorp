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

const TEST_URI = "data:text/html;charset=utf-8,<p id='gcli-input'>gcli-testMenu.js</p>";

function test() {
  helpers.addTabWithToolbar(TEST_URI, function(options) {
    return helpers.runTests(options, exports);
  }).then(finish);
}

// <INJECTED SOURCE:END>

'use strict';

// var helpers = require('gclitest/helpers');
// var mockCommands = require('gclitest/mockCommands');

exports.setup = function(options) {
  mockCommands.setup();
};

exports.shutdown = function(options) {
  mockCommands.shutdown();
};

exports.testOptions = function(options) {
  return helpers.audit(options, [
    {
      setup:    'tslong',
      check: {
        input:  'tslong',
        markup: 'VVVVVV',
        status: 'ERROR',
        hints: ' <msg> [options]',
        args: {
          msg: { value: undefined, status: 'INCOMPLETE' },
          num: { value: undefined, status: 'VALID' },
          sel: { value: undefined, status: 'VALID' },
          bool: { value: false, status: 'VALID' },
          bool2: { value: false, status: 'VALID' },
          sel2: { value: undefined, status: 'VALID' },
          num2: { value: undefined, status: 'VALID' }
        }
      }
    }
  ]);
};


// });

