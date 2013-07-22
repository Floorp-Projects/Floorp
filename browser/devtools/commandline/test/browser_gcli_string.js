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

const TEST_URI = "data:text/html;charset=utf-8,<p id='gcli-input'>gcli-testString.js</p>";

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

exports.testNewLine = function(options) {
  return helpers.audit(options, [
    {
      setup:    'echo a\\nb',
      check: {
        input:  'echo a\\nb',
        hints:            '',
        markup: 'VVVVVVVVV',
        cursor: 9,
        current: 'message',
        status: 'VALID',
        args: {
          command: { name: 'echo' },
          message: {
            value: 'a\nb',
            arg: ' a\\nb',
            status: 'VALID',
            message: ''
          }
        }
      }
    }
  ]);
};

exports.testTab = function(options) {
  return helpers.audit(options, [
    {
      setup:    'echo a\\tb',
      check: {
        input:  'echo a\\tb',
        hints:            '',
        markup: 'VVVVVVVVV',
        cursor: 9,
        current: 'message',
        status: 'VALID',
        args: {
          command: { name: 'echo' },
          message: {
            value: 'a\tb',
            arg: ' a\\tb',
            status: 'VALID',
            message: ''
          }
        }
      }
    }
  ]);
};

exports.testEscape = function(options) {
  return helpers.audit(options, [
    {
      // What's typed is actually:
      //         tsrsrsr a\\ b c
      setup:    'tsrsrsr a\\\\ b c',
      check: {
        input:  'tsrsrsr a\\\\ b c',
        hints:                 '',
        markup: 'VVVVVVVVVVVVVVV',
        status: 'VALID',
        message: '',
        args: {
          command: { name: 'tsrsrsr' },
          p1: { value: 'a\\', arg: ' a\\\\', status: 'VALID', message: '' },
          p2: { value: 'b', arg: ' b', status: 'VALID', message: '' },
          p3: { value: 'c', arg: ' c', status: 'VALID', message: '' },
        }
      }
    },
    {
      // What's typed is actually:
      //         tsrsrsr abc\\ndef asd asd
      setup:    'tsrsrsr abc\\\\ndef asd asd',
      check: {
        input:  'tsrsrsr abc\\\\ndef asd asd',
        hints:                           '',
        markup: 'VVVVVVVVVVVVVVVVVVVVVVVVV',
        status: 'VALID',
        message: '',
        args: {
          command: { name: 'tsrsrsr' },
          p1: {
            value: 'abc\\ndef',
            arg: ' abc\\\\ndef',
            status: 'VALID',
            message: ''
          },
          p2: { value: 'asd', arg: ' asd', status: 'VALID', message: '' },
          p3: { value: 'asd', arg: ' asd', status: 'VALID', message: '' },
        }
      }
    }
  ]);
};

exports.testBlank = function(options) {
  return helpers.audit(options, [
    {
      setup:    'tsrsrsr a "" c',
      check: {
        input:  'tsrsrsr a "" c',
        hints:                '',
        markup: 'VVVVVVVVVVVVVV',
        cursor: 14,
        current: 'p3',
        status: 'ERROR',
        message: '',
        args: {
          command: { name: 'tsrsrsr' },
          p1: {
            value: 'a',
            arg: ' a',
            status: 'VALID',
            message: ''
          },
          p2: {
            value: undefined,
            arg: ' ""',
            status: 'INCOMPLETE',
            message: ''
          },
          p3: {
            value: 'c',
            arg: ' c',
            status: 'VALID',
            message: ''
          }
        }
      }
    },
    {
      setup:    'tsrsrsr a b ""',
      check: {
        input:  'tsrsrsr a b ""',
        hints:                '',
        markup: 'VVVVVVVVVVVVVV',
        cursor: 14,
        current: 'p3',
        status: 'VALID',
        message: '',
        args: {
          command: { name: 'tsrsrsr' },
          p1: {
            value: 'a',
            arg: ' a',
            status:'VALID',
            message: '' },
          p2: {
            value: 'b',
            arg: ' b',
            status: 'VALID',
            message: ''
          },
          p3: {
            value: '',
            arg: ' ""',
            status: 'VALID',
            message: ''
          }
        }
      }
    }
  ]);
};

exports.testBlankWithParam = function(options) {
  return helpers.audit(options, [
    {
      setup:    'tsrsrsr  a --p3',
      check: {
        input:  'tsrsrsr  a --p3',
        hints:                 ' <string> <p2>',
        markup: 'VVVVVVVVVVVVVVV',
        cursor: 15,
        current: 'p3',
        status: 'ERROR',
        message: '',
        args: {
          command: { name: 'tsrsrsr' },
          p1: { value: 'a', arg: '  a', status: 'VALID', message: '' },
          p2: { value: undefined, arg: '', status: 'INCOMPLETE', message: '' },
          p3: { value: '', arg: ' --p3', status: 'VALID', message: '' },
        }
      }
    },
    {
      setup:    'tsrsrsr  a --p3 ',
      check: {
        input:  'tsrsrsr  a --p3 ',
        hints:                  '<string> <p2>',
        markup: 'VVVVVVVVVVVVVVVV',
        cursor: 16,
        current: 'p3',
        status: 'ERROR',
        message: '',
        args: {
          command: { name: 'tsrsrsr' },
          p1: { value: 'a', arg: '  a', status: 'VALID', message: '' },
          p2: { value: undefined, arg: '', status: 'INCOMPLETE', message: '' },
          p3: { value: '', arg: ' --p3 ', status: 'VALID', message: '' },
        }
      }
    },
    {
      setup:    'tsrsrsr  a --p3 "',
      check: {
        input:  'tsrsrsr  a --p3 "',
        hints:                   ' <p2>',
        markup: 'VVVVVVVVVVVVVVVVV',
        cursor: 17,
        current: 'p3',
        status: 'ERROR',
        message: '',
        args: {
          command: { name: 'tsrsrsr' },
          p1: { value: 'a', arg: '  a', status: 'VALID', message: '' },
          p2: { value: undefined, arg: '', status: 'INCOMPLETE', message: '' },
          p3: { value: '', arg: ' --p3 "', status: 'VALID', message: '' },
        }
      }
    },
    {
      setup:    'tsrsrsr  a --p3 ""',
      check: {
        input:  'tsrsrsr  a --p3 ""',
        hints:                    ' <p2>',
        markup: 'VVVVVVVVVVVVVVVVVV',
        cursor: 18,
        current: 'p3',
        status: 'ERROR',
        message: '',
        args: {
          command: { name: 'tsrsrsr' },
          p1: { value: 'a', arg: '  a', status: 'VALID', message: '' },
          p2: { value: undefined, arg: '', status: 'INCOMPLETE', message: '' },
          p3: { value: '', arg: ' --p3 ""', status: 'VALID', message: '' },
        }
      }
    }
  ]);
};


// });
