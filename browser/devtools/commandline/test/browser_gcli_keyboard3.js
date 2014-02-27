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

var TEST_URI = "data:text/html;charset=utf-8,<p id='gcli-input'>gcli-testKeyboard3.js</p>";

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

exports.testDecr = function(options) {
  return helpers.audit(options, [
    /*
    // See notes at top of testIncr in testKeyboard2.js
    {
      setup: 'tsu -70<DOWN>',
      check: { input: 'tsu -5' }
    },
    {
      setup: 'tsu -7<DOWN>',
      check: { input: 'tsu -5' }
    },
    {
      setup: 'tsu -6<DOWN>',
      check: { input: 'tsu -5' }
    },
    */
    {
      setup: 'tsu -5<DOWN>',
      check: { input: 'tsu -5' }
    },
    {
      setup: 'tsu -4<DOWN>',
      check: { input: 'tsu -5' }
    },
    {
      setup: 'tsu -3<DOWN>',
      check: { input: 'tsu -5' }
    },
    {
      setup: 'tsu -2<DOWN>',
      check: { input: 'tsu -3' }
    },
    {
      setup: 'tsu -1<DOWN>',
      check: { input: 'tsu -3' }
    },
    {
      setup: 'tsu 0<DOWN>',
      check: { input: 'tsu -3' }
    },
    {
      setup: 'tsu 1<DOWN>',
      check: { input: 'tsu 0' }
    },
    {
      setup: 'tsu 2<DOWN>',
      check: { input: 'tsu 0' }
    },
    {
      setup: 'tsu 3<DOWN>',
      check: { input: 'tsu 0' }
    },
    {
      setup: 'tsu 4<DOWN>',
      check: { input: 'tsu 3' }
    },
    {
      setup: 'tsu 5<DOWN>',
      check: { input: 'tsu 3' }
    },
    {
      setup: 'tsu 6<DOWN>',
      check: { input: 'tsu 3' }
    },
    {
      setup: 'tsu 7<DOWN>',
      check: { input: 'tsu 6' }
    },
    {
      setup: 'tsu 8<DOWN>',
      check: { input: 'tsu 6' }
    },
    {
      setup: 'tsu 9<DOWN>',
      check: { input: 'tsu 6' }
    },
    {
      setup: 'tsu 10<DOWN>',
      check: { input: 'tsu 9' }
    }
    /*
    // See notes at top of testIncr
    {
      setup: 'tsu 100<DOWN>',
      check: { input: 'tsu 9' }
    }
    */
  ]);
};
