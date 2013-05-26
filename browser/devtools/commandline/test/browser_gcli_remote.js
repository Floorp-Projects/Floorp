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

const TEST_URI = "data:text/html;charset=utf-8,<p id='gcli-input'>gcli-testRemote.js</p>";

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

exports.testRemote = function(options) {
  return helpers.audit(options, [
    {
      skipRemainingIf: !options.isHttp,
      setup:    'remote ',
      check: {
        input:  'remote ',
        hints:         '',
        markup: 'EEEEEEV',
        cursor: 7,
        current: '__command',
        status: 'ERROR',
        options: [ ],
        message: 'Can\'t use \'remote\'.',
        predictions: [ ],
        unassigned: [ ],
      }
    },
    {
      setup:    'connect remote',
      check: {
        input:  'connect remote',
        hints:                ' [options]',
        markup: 'VVVVVVVVVVVVVV',
        cursor: 14,
        current: 'prefix',
        status: 'VALID',
        options: [ ],
        message: '',
        predictions: [ ],
        unassigned: [ ],
        args: {
          command: { name: 'connect' },
          prefix: { value: 'remote', arg: ' remote', status: 'VALID', message: '' },
          host: { value: undefined, arg: '', status: 'VALID', message: '' },
          port: { value: undefined, arg: '', status: 'VALID', message: '' },
        }
      },
      exec: {
        output: /^Added [0-9]* commands.$/,
        completed: false,
        type: 'string',
        error: false
      }
    },
    {
      setup:    'remote ',
      check: {
        input:  'remote ',
        hints:         '',
        markup: 'IIIIIIV',
        cursor: 7,
        current: '__command',
        status: 'ERROR',
        optionsIncludes: [
          'remote', 'remote cd', 'remote context', 'remote echo',
          'remote exec', 'remote exit', 'remote firefox', 'remote help',
          'remote intro', 'remote make'
        ],
        message: '',
        predictions: [ 'remote' ],
        unassigned: [ ],
      }
    },
    {
      setup:    'remote echo hello world',
      check: {
        input:  'remote echo hello world',
        hints:                         '',
        markup: 'VVVVVVVVVVVVVVVVVVVVVVV',
        cursor: 23,
        current: 'message',
        status: 'VALID',
        options: [ ],
        message: '',
        predictions: [ ],
        unassigned: [ ],
        args: {
          command: { name: 'remote echo' },
          message: {
            value: 'hello world',
            arg: ' hello world',
            status: 'VALID',
            message: ''
          }
        }
      },
      exec: {
        output: 'hello world',
        completed: false,
        type: 'string',
        error: false
      }
    },
    {
      setup:    'remote exec ls',
      check: {
        input:  'remote exec ls',
        hints:                '',
        markup: 'VVVVVVVVVVVVVV',
        cursor: 14,
        current: 'command',
        status: 'VALID',
        options: [ ],
        message: '',
        predictions: [ ],
        unassigned: [ ],
        args: {
          command: {
            value: 'ls',
            arg: ' ls',
            status: 'VALID',
            message: ''
          }
        }
      },
      exec: {
        // output: '', We can't rely on the contents of the FS
        completed: false,
        type: 'string',
        error: false
      }
    },
    {
      setup:    'remote sleep mistake',
      check: {
        input:  'remote sleep mistake',
        hints:                      '',
        markup: 'VVVVVVVVVVVVVEEEEEEE',
        cursor: 20,
        current: 'length',
        status: 'ERROR',
        options: [ ],
        message: 'Can\'t convert "mistake" to a number.',
        predictions: [ ],
        unassigned: [ ],
        args: {
          command: { name: 'remote sleep' },
          length: {
            value: undefined,
            arg: ' mistake',
            status: 'ERROR',
            message: 'Can\'t convert "mistake" to a number.'
          }
        }
      }
    },
    {
      setup:    'remote sleep 1',
      check: {
        input:  'remote sleep 1',
        hints:                 '',
        markup: 'VVVVVVVVVVVVVV',
        cursor: 14,
        current: 'length',
        status: 'VALID',
        options: [ ],
        message: '',
        predictions: [ ],
        unassigned: [ ],
        args: {
          command: { name: 'remote sleep' },
          length: { value: 1, arg: ' 1', status: 'VALID', message: '' }
        }
      },
      exec: {
        output: 'Done',
        completed: false,
        type: 'string',
        error: false
      }
    },
    {
      setup:    'remote help ',
      skipIf: true, // The help command is not remotable
      check: {
        input:  'remote help ',
        hints:              '[search]',
        markup: 'VVVVVVVVVVVV',
        cursor: 12,
        current: 'search',
        status: 'VALID',
        options: [ ],
        message: '',
        predictions: [ ],
        unassigned: [ ],
        args: {
          command: { name: 'remote help' },
          search: {
            value: undefined,
            arg: '',
            status: 'VALID',
            message: ''
          }
        }
      },
      exec: {
        output: '',
        completed: false,
        type: 'string',
        error: false
      }
    },
    {
      setup:    'remote intro',
      check: {
        input:  'remote intro',
        hints:              '',
        markup: 'VVVVVVVVVVVV',
        cursor: 12,
        current: '__command',
        status: 'VALID',
        options: [ ],
        message: '',
        predictions: [ ],
        unassigned: [ ],
        args: {
          command: { name: 'remote intro' }
        }
      },
      exec: {
        output: [
          /^This command line/,
          /F1\/Escape/
        ],
        completed: false,
        type: 'intro',
        error: false
      }
    },
    {
      setup:    'context remote',
      check: {
        input:  'context remote',
        hints:                '',
        markup: 'VVVVVVVVVVVVVV',
        cursor: 14,
        current: 'prefix',
        status: 'VALID',
        optionsContains: [ 'remote', 'remote cd', 'remote echo', 'remote exec', 'remote exit', 'remote firefox', 'remote help', 'remote intro', 'remote make' ],
        message: '',
        predictionsContains: [ 'remote', 'remote cd', 'remote echo', 'remote exec', 'remote exit', 'remote firefox', 'remote help', 'remote intro', 'remote make', 'remote pref' ],
        unassigned: [ ],
        args: {
          command: { name: 'context' },
          prefix: {
            /*value:[object Object],*/
            arg: ' remote',
            status: 'VALID',
            message: ''
          }
        }
      },
      exec: {
        output: 'Using remote as a command prefix',
        completed: true,
        type: 'string',
        error: false
      }
    },
    {
      setup:    'exec ls',
      check: {
        input:  'exec ls',
        hints:         '',
        markup: 'VVVVVVV',
        cursor: 7,
        current: 'command',
        status: 'VALID',
        options: [ ],
        message: '',
        predictions: [ ],
        unassigned: [ ],
        args: {
          command: { value: 'ls', arg: ' ls', status: 'VALID', message: '' },
        }
      },
      exec: {
        // output: '', We can't rely on the contents of the filesystem
        completed: false,
        type: 'string',
        error: false
      }
    },
    {
      setup:    'echo hello world',
      check: {
        input:  'echo hello world',
        hints:                  '',
        markup: 'VVVVVVVVVVVVVVVV',
        cursor: 16,
        current: 'message',
        status: 'VALID',
        options: [ ],
        message: '',
        predictions: [ ],
        unassigned: [ ],
        args: {
          command: { name: 'remote echo' },
          message: {
            value: 'hello world',
            arg: ' hello world',
            status: 'VALID',
            message: ''
          }
        }
      },
      exec: {
        output: /^hello world$/,
        type: 'string',
        error: false
      }
    },
    {
      setup:    'context',
      check: {
        input:  'context',
        hints:         ' [prefix]',
        markup: 'VVVVVVV',
        cursor: 7,
        current: '__command',
        status: 'VALID',
        optionsContains: [ 'remote', 'remote cd', 'remote echo', 'remote exec', 'remote exit', 'remote firefox', 'remote help', 'remote intro', 'remote make' ],
        message: '',
        predictions: [ ],
        unassigned: [ ],
        args: {
          command: { name: 'context' },
          prefix: { value: undefined, arg: '', status: 'VALID', message: '' }
        }
      },
      exec: {
        output: 'Command prefix is unset',
        completed: true,
        type: 'string',
        error: false
      }
    },
    {
      setup:    'disconnect ',
      check: {
        input:  'disconnect ',
        hints:             'remote',
        markup: 'VVVVVVVVVVV',
        cursor: 11,
        current: 'prefix',
        status: 'ERROR',
        options: [ 'remote' ],
        message: '',
        predictions: [ 'remote' ],
        unassigned: [ ],
        args: {
          command: { name: 'disconnect' },
          prefix: {
            value: undefined,
            arg: '',
            status: 'INCOMPLETE',
            message: ''
          }
        }
      }
    },
    {
      setup:    'disconnect remote --force',
      check: {
        input:  'disconnect remote --force',
        hints:                           '',
        markup: 'VVVVVVVVVVVVVVVVVVVVVVVVV',
        cursor: 25,
        current: 'force',
        status: 'VALID',
        message: '',
        unassigned: [ ],
        args: {
          command: { name: 'disconnect' },
          prefix: {
            value: function(connection) {
              assert.is(connection.prefix, 'remote', 'disconnecting remote');
            },
            arg: ' remote',
            status: 'VALID',
            message: ''
          }
        }
      },
      exec: {
        output: /^Removed [0-9]* commands.$/,
        completed: true,
        type: 'string',
        error: false
      }
    },
    {
      setup:    'remote ',
      check: {
        input:  'remote ',
        hints:         '',
        markup: 'EEEEEEV',
        cursor: 7,
        current: '__command',
        status: 'ERROR',
        options: [ ],
        message: 'Can\'t use \'remote\'.',
        predictions: [ ],
        unassigned: [ ],
      }
    }
  ]);
};


// });

