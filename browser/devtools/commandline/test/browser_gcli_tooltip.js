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

const TEST_URI = "data:text/html;charset=utf-8,<p id='gcli-input'>gcli-testTooltip.js</p>";

function test() {
  helpers.addTabWithToolbar(TEST_URI, function(options) {
    return helpers.runTests(options, exports);
  }).then(finish);
}

// <INJECTED SOURCE:END>

'use strict';

// var assert = require('test/assert');
// var helpers = require('gclitest/helpers');
// var mockCommands = require('gclitest/mockCommands');

exports.setup = function(options) {
  mockCommands.setup();
};

exports.shutdown = function(options) {
  mockCommands.shutdown();
};

exports.testActivate = function(options) {
  if (!options.display) {
    assert.log('No display. Skipping activate tests');
    return;
  }

  if (options.isJsdom) {
    assert.log('Reduced checks due to JSDom.textContent');
  }

  return helpers.audit(options, [
    {
      setup:    ' ',
      check: {
        input:  ' ',
        hints:   '',
        markup: 'V',
        cursor: 1,
        current: '__command',
        status: 'ERROR',
        message:  '',
        unassigned: [ ],
        outputState: 'false:default',
        tooltipState: 'false:default'
      }
    },
    {
      setup:    'tsb ',
      check: {
        input:  'tsb ',
        hints:      'false',
        markup: 'VVVV',
        cursor: 4,
        current: 'toggle',
        status: 'VALID',
        options: [ 'false', 'true' ],
        message:  '',
        predictions: [ 'false', 'true' ],
        unassigned: [ ],
        outputState: 'false:default',
        tooltipState: 'true:importantFieldFlag'
      }
    },
    {
      setup:    'tsb t',
      check: {
        input:  'tsb t',
        hints:       'rue',
        markup: 'VVVVI',
        cursor: 5,
        current: 'toggle',
        status: 'ERROR',
        options: [ 'true' ],
        message:  '',
        predictions: [ 'true' ],
        unassigned: [ ],
        outputState: 'false:default',
        tooltipState: 'true:importantFieldFlag'
      }
    },
    {
      setup:    'tsb tt',
      check: {
        input:  'tsb tt',
        hints:        ' -> true',
        markup: 'VVVVII',
        cursor: 6,
        current: 'toggle',
        status: 'ERROR',
        options: [ 'true' ],
        message: '',
        predictions: [ 'true' ],
        unassigned: [ ],
        outputState: 'false:default',
        tooltipState: 'true:importantFieldFlag'
      }
    },
    {
      setup:    'wxqy',
      check: {
        input:  'wxqy',
        hints:      '',
        markup: 'EEEE',
        cursor: 4,
        current: '__command',
        status: 'ERROR',
        options: [ ],
        message:  'Can\'t use \'wxqy\'.',
        predictions: [ ],
        unassigned: [ ],
        outputState: 'false:default',
        tooltipState: 'true:isError'
      }
    },
    {
      setup:    '',
      check: {
        input:  '',
        hints:  '',
        markup: '',
        cursor: 0,
        current: '__command',
        status: 'ERROR',
        message: '',
        unassigned: [ ],
        outputState: 'false:default',
        tooltipState: 'false:default'
      }
    }
  ]);
};

// });
