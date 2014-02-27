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

var TEST_URI = "data:text/html;charset=utf-8,<p id='gcli-input'>gcli-testCli1.js</p>";

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

// var assert = require('../testharness/assert');
// var helpers = require('./helpers');

exports.testBlank = function(options) {
  return helpers.audit(options, [
    {
      setup:    '',
      check: {
        input:  '',
        hints:  '',
        markup: '',
        cursor: 0,
        current: '__command',
        status: 'ERROR'
      },
      post: function() {
        assert.is(options.requisition.commandAssignment.value, undefined);
      }
    },
    {
      setup:    ' ',
      check: {
        input:  ' ',
        hints:   '',
        markup: 'V',
        cursor: 1,
        current: '__command',
        status: 'ERROR'
      },
      post: function() {
        assert.is(options.requisition.commandAssignment.value, undefined);
      }
    },
    {
      name: '| ',
      setup: function() {
        return helpers.setInput(options, ' ', 0);
      },
      check: {
        input:  ' ',
        hints:   '',
        markup: 'V',
        cursor: 0,
        current: '__command',
        status: 'ERROR'
      },
      post: function() {
        assert.is(options.requisition.commandAssignment.value, undefined);
      }
    }
  ]);
};

exports.testDelete = function(options) {
  return helpers.audit(options, [
    {
      setup:    'x<BACKSPACE>',
      check: {
        input:  '',
        hints:  '',
        markup: '',
        cursor: 0,
        current: '__command',
        status: 'ERROR'
      },
      post: function() {
        assert.is(options.requisition.commandAssignment.value, undefined);
      }
    }
  ]);
};

exports.testIncompleteMultiMatch = function(options) {
  return helpers.audit(options, [
    {
      setup:    'tsn ex',
      check: {
        input:  'tsn ex',
        hints:        't',
        markup: 'IIIVII',
        cursor: 6,
        current: '__command',
        status: 'ERROR',
        predictionsContains: [
          'tsn ext', 'tsn exte', 'tsn exten', 'tsn extend'
        ]
      }
    }
  ]);
};

exports.testIncompleteSingleMatch = function(options) {
  return helpers.audit(options, [
    {
      setup:    'tselar',
      check: {
        input:  'tselar',
        hints:        'r',
        markup: 'IIIIII',
        cursor: 6,
        current: '__command',
        status: 'ERROR',
        predictions: [ 'tselarr' ],
        unassigned: [ ]
      }
    }
  ]);
};

exports.testTsv = function(options) {
  return helpers.audit(options, [
    {
      setup:    'tsv',
      check: {
        input:  'tsv',
        hints:     ' <optionType> <optionValue>',
        markup: 'VVV',
        cursor: 3,
        current: '__command',
        status: 'ERROR',
        predictions: [ ],
        unassigned: [ ],
        args: {
          command: { name: 'tsv' },
          optionType: { arg: '', status: 'INCOMPLETE' },
          optionValue: { arg: '', status: 'INCOMPLETE' }
        }
      }
    },
    {
      setup:    'tsv ',
      check: {
        input:  'tsv ',
        hints:      'option1 <optionValue>',
        markup: 'VVVV',
        cursor: 4,
        current: 'optionType',
        status: 'ERROR',
        predictions: [ 'option1', 'option2', 'option3' ],
        unassigned: [ ],
        tooltipState: 'true:importantFieldFlag',
        args: {
          command: { name: 'tsv' },
          optionType: { arg: '', status: 'INCOMPLETE' },
          optionValue: { arg: '', status: 'INCOMPLETE' }
        }
      }
    },
    {
      name: 'ts|v',
      setup: function() {
        return helpers.setInput(options, 'tsv ', 2);
      },
      check: {
        input:  'tsv ',
        hints:      '<optionType> <optionValue>',
        markup: 'VVVV',
        cursor: 2,
        current: '__command',
        status: 'ERROR',
        predictions: [ ],
        unassigned: [ ],
        args: {
          command: { name: 'tsv' },
          optionType: { arg: '', status: 'INCOMPLETE' },
          optionValue: { arg: '', status: 'INCOMPLETE' }
        }
      }
    },
    {
      setup:    'tsv o',
      check: {
        input:  'tsv o',
        hints:       'ption1 <optionValue>',
        markup: 'VVVVI',
        cursor: 5,
        current: 'optionType',
        status: 'ERROR',
        predictions: [ 'option1', 'option2', 'option3' ],
        unassigned: [ ],
        tooltipState: 'true:importantFieldFlag',
        args: {
          command: { name: 'tsv' },
          optionType: {
            value: undefined,
            arg: ' o',
            status: 'INCOMPLETE',
            message: 'Value required for \'optionType\'.'
          },
          optionValue: {
            value: undefined,
            arg: '',
            status: 'INCOMPLETE',
            message: 'Value required for \'optionValue\'.'
          }
        }
      }
    },
    {
      setup:    'tsv option',
      check: {
        input:  'tsv option',
        hints:            '1 <optionValue>',
        markup: 'VVVVIIIIII',
        cursor: 10,
        current: 'optionType',
        status: 'ERROR',
        predictions: [ 'option1', 'option2', 'option3' ],
        unassigned: [ ],
        tooltipState: 'true:importantFieldFlag',
        args: {
          command: { name: 'tsv' },
          optionType: {
            value: undefined,
            arg: ' option',
            status: 'INCOMPLETE',
            message: 'Value required for \'optionType\'.'
          },
          optionValue: {
            value: undefined,
            arg: '',
            status: 'INCOMPLETE',
            message: 'Value required for \'optionValue\'.'
          }
        }
      }
    },
    {
      skipRemainingIf: options.isNoDom,
      name: '|tsv option',
      setup: function() {
        return helpers.setInput(options, 'tsv option', 0);
      },
      check: {
        input:  'tsv option',
        hints:            ' <optionValue>',
        markup: 'VVVVEEEEEE',
        cursor: 0,
        current: '__command',
        status: 'ERROR',
        predictions: [ ],
        unassigned: [ ],
        args: {
          command: { name: 'tsv' },
          optionType: {
            value: undefined,
            arg: ' option',
            status: 'INCOMPLETE',
            message: 'Value required for \'optionType\'.'
          },
          optionValue: {
            value: undefined,
            arg: '',
            status: 'INCOMPLETE',
            message: 'Value required for \'optionValue\'.'
          }
        }
      }
    },
    {
      setup:    'tsv option ',
      check: {
        input:  'tsv option ',
        hints:             '<optionValue>',
        markup: 'VVVVEEEEEEV',
        cursor: 11,
        current: 'optionValue',
        status: 'ERROR',
        predictions: [ ],
        unassigned: [ ],
        tooltipState: 'false:default',
        args: {
          command: { name: 'tsv' },
          optionType: {
            value: undefined,
            arg: ' option ',
            status: 'ERROR',
            message: 'Can\'t use \'option\'.'
          },
          optionValue: {
            value: undefined,
            arg: '',
            status: 'INCOMPLETE',
            message: 'Value required for \'optionValue\'.'
          }
        }
      }
    },
    {
      setup:    'tsv option1',
      check: {
        input:  'tsv option1',
        hints:             ' <optionValue>',
        markup: 'VVVVVVVVVVV',
        cursor: 11,
        current: 'optionType',
        status: 'ERROR',
        predictions: [ 'option1' ],
        unassigned: [ ],
        tooltipState: 'true:importantFieldFlag',
        args: {
          command: { name: 'tsv' },
          optionType: {
            value: 'string',
            arg: ' option1',
            status: 'VALID',
            message: ''
          },
          optionValue: {
            value: undefined,
            arg: '',
            status: 'INCOMPLETE',
            message: 'Value required for \'optionValue\'.'
          }
        }
      }
    },
    {
      setup:    'tsv option1 ',
      check: {
        input:  'tsv option1 ',
        hints:              '<optionValue>',
        markup: 'VVVVVVVVVVVV',
        cursor: 12,
        current: 'optionValue',
        status: 'ERROR',
        predictions: [ ],
        unassigned: [ ],
        args: {
          command: { name: 'tsv' },
          optionType: {
            value: 'string',
            arg: ' option1 ',
            status: 'VALID',
            message: ''
          },
          optionValue: {
            value: undefined,
            arg: '',
            status: 'INCOMPLETE',
            message: 'Value required for \'optionValue\'.'
          }
        }
      }
    },
    {
      setup:    'tsv option2',
      check: {
        input:  'tsv option2',
        hints:             ' <optionValue>',
        markup: 'VVVVVVVVVVV',
        cursor: 11,
        current: 'optionType',
        status: 'ERROR',
        predictions: [ 'option2' ],
        unassigned: [ ],
        tooltipState: 'true:importantFieldFlag',
        args: {
          command: { name: 'tsv' },
          optionType: {
            value: 'number',
            arg: ' option2',
            status: 'VALID',
            message: ''
          },
          optionValue: {
            value: undefined,
            arg: '',
            status: 'INCOMPLETE',
            message: 'Value required for \'optionValue\'.'
          }
        }
      }
    }
  ]);
};

exports.testTsvValues = function(options) {
  return helpers.audit(options, [
    {
      setup:    'tsv option1 6',
      check: {
        input:  'tsv option1 6',
        hints:               '',
        markup: 'VVVVVVVVVVVVV',
        cursor: 13,
        current: 'optionValue',
        status: 'VALID',
        predictions: [ ],
        unassigned: [ ],
        args: {
          command: { name: 'tsv' },
          optionType: {
            value: 'string',
            arg: ' option1',
            status: 'VALID',
            message: ''
          },
          optionValue: {
            arg: ' 6',
            status: 'VALID',
            message: ''
          }
        }
      }
    },
    {
      setup:    'tsv option2 6',
      check: {
        input:  'tsv option2 6',
        hints:               '',
        markup: 'VVVVVVVVVVVVV',
        cursor: 13,
        current: 'optionValue',
        status: 'VALID',
        predictions: [ ],
        unassigned: [ ],
        args: {
          command: { name: 'tsv' },
          optionType: {
            value: 'number',
            arg: ' option2',
            status: 'VALID',
            message: ''
          },
          optionValue: {
            arg: ' 6',
            status: 'VALID',
            message: ''
          }
        }
      }
    },
    // Delegated remote types can't transfer value types so we only test for
    // the value of 'value' when we're local
    {
      skipIf: options.isRemote,
      setup: 'tsv option1 6',
      check: {
        args: {
          optionValue: { value: '6' }
        }
      }
    },
    {
      skipIf: options.isRemote,
      setup: 'tsv option2 6',
      check: {
        args: {
          optionValue: { value: 6 }
        }
      }
    }
  ]);
};

exports.testInvalid = function(options) {
  return helpers.audit(options, [
    {
      setup:    'zxjq',
      check: {
        input:  'zxjq',
        hints:      '',
        markup: 'EEEE',
        cursor: 4,
        current: '__command',
        status: 'ERROR',
        predictions: [ ],
        unassigned: [ ],
        tooltipState: 'true:isError'
      }
    },
    {
      setup:    'zxjq ',
      check: {
        input:  'zxjq ',
        hints:       '',
        markup: 'EEEEV',
        cursor: 5,
        current: '__command',
        status: 'ERROR',
        predictions: [ ],
        unassigned: [ ],
        tooltipState: 'true:isError'
      }
    },
    {
      setup:    'zxjq one',
      check: {
        input:  'zxjq one',
        hints:          '',
        markup: 'EEEEVEEE',
        cursor: 8,
        current: '__unassigned',
        status: 'ERROR',
        predictions: [ ],
        unassigned: [ ' one' ],
        tooltipState: 'true:isError'
      }
    }
  ]);
};
