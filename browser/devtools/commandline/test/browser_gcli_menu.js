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

var TEST_URI = "data:text/html;charset=utf-8,<p id='gcli-input'>gcli-testMenu.js</p>";

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

// var helpers = require('./helpers');

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
