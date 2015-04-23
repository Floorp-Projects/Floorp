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
  helpers.runTestModule(exports, "browser_gcli_tooltip.js");
}

// var assert = require('../testharness/assert');
// var helpers = require('./helpers');

exports.testActivate = function(options) {
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
