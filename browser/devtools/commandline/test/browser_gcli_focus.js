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
  helpers.runTestModule(exports, "browser_gcli_focus.js");
}

// var helpers = require('./helpers');

exports.testBasic = function(options) {
  return helpers.audit(options, [
    {
      name: 'exec setup',
      setup: function() {
        // Just check that we've got focus, and everything is clear
        helpers.focusInput(options);
        return helpers.setInput(options, 'echo hi');
      },
      check: { },
      exec: { }
    },
    {
      setup:    'tsn deep',
      check: {
        input:  'tsn deep',
        hints:          ' down nested cmd',
        markup: 'IIIVIIII',
        cursor: 8,
        status: 'ERROR',
        outputState: 'false:default',
        tooltipState: 'false:default'
      }
    },
    {
      setup:    'tsn deep<TAB>',
      check: {
        input:  'tsn deep down nested cmd ',
        hints:                           '',
        markup: 'VVVVVVVVVVVVVVVVVVVVVVVVV',
        cursor: 25,
        status: 'VALID',
        outputState: 'false:default',
        tooltipState: 'false:default'
      }
    }
  ]);
};
