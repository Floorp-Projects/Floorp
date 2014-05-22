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

var TEST_URI = "data:text/html;charset=utf-8,<p id='gcli-input'>gcli-testFail.js</p>";

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

exports.testBasic = function(options) {
  return helpers.audit(options, [
    {
      setup: 'tsfail reject',
      exec: {
        output: 'rejected promise',
        type: 'error',
        error: true
      }
    },
    {
      setup: 'tsfail rejecttyped',
      exec: {
        output: '54',
        type: 'number',
        error: true
      }
    },
    {
      setup: 'tsfail throwerror',
      exec: {
        output: /thrown error$/,
        type: 'error',
        error: true
      }
    },
    {
      setup: 'tsfail throwstring',
      exec: {
        output: 'thrown string',
        type: 'error',
        error: true
      }
    },
    {
      setup: 'tsfail noerror',
      exec: {
        output: 'no error',
        type: 'string',
        error: false
      }
    }
  ]);
};
