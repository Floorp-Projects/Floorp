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

var TEST_URI = "data:text/html;charset=utf-8,<p id='gcli-input'>gcli-testShort.js</p>";

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
      setup:    'tshidden -v',
      check: {
        input:  'tshidden -v',
        hints:             ' <string>',
        markup: 'VVVVVVVVVII',
        cursor: 11,
        current: 'visible',
        status: 'ERROR',
        options: [ ],
        message: '',
        predictions: [ ],
        unassigned: [ ],
        args: {
          command: { name: 'tshidden' },
          visible: {
            value: undefined,
            arg: ' -v',
            status: 'INCOMPLETE'
          },
          invisiblestring: {
            value: undefined,
            arg: '',
            status: 'VALID',
            message: ''
          },
          invisibleboolean: {
            value: false,
            arg: '',
            status: 'VALID',
            message: ''
          }
        }
      }
    },
    {
      setup:    'tshidden -v v',
      check: {
        input:  'tshidden -v v',
        hints:               '',
        markup: 'VVVVVVVVVVVVV',
        cursor: 13,
        current: 'visible',
        status: 'VALID',
        options: [ ],
        message: '',
        predictions: [ ],
        unassigned: [ ],
        args: {
          command: { name: 'tshidden' },
          visible: {
            value: 'v',
            arg: ' -v v',
            status: 'VALID',
            message: ''
          },
          invisiblestring: {
            value: undefined,
            arg: '',
            status: 'VALID',
            message: ''
          },
          invisibleboolean: {
            value: false,
            arg: '',
            status: 'VALID',
            message: ''
          }
        }
      }
    },
    {
      setup:    'tshidden -i i',
      check: {
        input:  'tshidden -i i',
        hints:               ' [options]',
        markup: 'VVVVVVVVVVVVV',
        cursor: 13,
        current: 'invisiblestring',
        status: 'VALID',
        options: [ ],
        message: '',
        predictions: [ ],
        unassigned: [ ],
        args: {
          command: { name: 'tshidden' },
          visible: {
            value: undefined,
            arg: '',
            status: 'VALID',
            message: ''
          },
          invisiblestring: {
            value: 'i',
            arg: ' -i i',
            status: 'VALID',
            message: ''
          },
          invisibleboolean: {
            value: false,
            arg: '',
            status: 'VALID',
            message: ''
          }
        }
      }
    },
    {
      setup:    'tshidden -b',
      check: {
        input:  'tshidden -b',
        hints:             ' [options]',
        markup: 'VVVVVVVVVVV',
        cursor: 11,
        current: 'invisibleboolean',
        status: 'VALID',
        options: [ ],
        message: '',
        predictions: [ ],
        unassigned: [ ],
        args: {
          command: { name: 'tshidden' },
          visible: {
            value: undefined,
            arg: '',
            status: 'VALID',
            message: ''
          },
          invisiblestring: {
            value: undefined,
            arg: '',
            status: 'VALID',
            message: ''
          },
          invisibleboolean: {
            value: true,
            arg: ' -b',
            status: 'VALID',
            message: ''
          }
        }
      }
    },
    {
      setup:    'tshidden -j',
      check: {
        input:  'tshidden -j',
        hints:             ' [options]',
        markup: 'VVVVVVVVVEE',
        cursor: 11,
        current: '__unassigned',
        status: 'ERROR',
        options: [ ],
        message: 'Can\'t use \'-j\'.',
        predictions: [ ],
        unassigned: [ ' -j' ],
        args: {
          command: { name: 'tshidden' },
          visible: {
            value: undefined,
            arg: '',
            status: 'VALID',
            message: ''
          },
          invisiblestring: {
            value: undefined,
            arg: '',
            status: 'VALID',
            message: ''
          },
          invisibleboolean: {
            value: false,
            arg: '',
            status: 'VALID',
            message: ''
          }
        }
      }
    },
    {
      setup:    'tshidden -v jj -b --',
      check: {
        input:  'tshidden -v jj -b --',
        hints:                      '',
        markup: 'VVVVVVVVVVVVVVVVVVEE',
        cursor: 20,
        current: '__unassigned',
        status: 'ERROR',
        options: [ ],
        message: 'Can\'t use \'--\'.',
        predictions: [ ],
        unassigned: [ ' --' ],
        args: {
          command: { name: 'tshidden' },
          visible: {
            value: 'jj',
            arg: ' -v jj',
            status: 'VALID',
            message: ''
          },
          invisiblestring: {
            value: undefined,
            arg: '',
            status: 'VALID',
            message: ''
          },
          invisibleboolean: {
            value: true,
            arg: ' -b',
            status: 'VALID',
            message: ''
          }
        }
      }
    }
  ]);
};
