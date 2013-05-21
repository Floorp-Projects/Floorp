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

const TEST_URI = "data:text/html;charset=utf-8,<p id='gcli-input'>gcli-testCli.js</p>";

function test() {
  helpers.addTabWithToolbar(TEST_URI, function(options) {
    return helpers.runTests(options, exports);
  }).then(finish);
}

// <INJECTED SOURCE:END>

'use strict';

// var helpers = require('gclitest/helpers');
// var mockCommands = require('gclitest/mockCommands');

// var assert = require('test/assert');

exports.setup = function(options) {
  mockCommands.setup();
};

exports.shutdown = function(options) {
  mockCommands.shutdown();
};

exports.testBlank = function(options) {
  var requisition = options.display.requisition;

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
        assert.is(requisition.commandAssignment.value, undefined);
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
        assert.is(requisition.commandAssignment.value, undefined);
      }
    },
    {
      name: '| ',
      setup: function() {
        helpers.setInput(options, ' ', 0);
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
        assert.is(requisition.commandAssignment.value, undefined);
      }
    }
  ]);
};

exports.testIncompleteMultiMatch = function(options) {
  return helpers.audit(options, [
    {
      setup:    't',
      skipIf: options.isFirefox, // 't' hints at 'tilt' in firefox
      check: {
        input:  't',
        hints:   'est',
        markup: 'I',
        cursor: 1,
        current: '__command',
        status: 'ERROR',
        predictionsContains: [ 'tsb' ]
      }
    },
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
          optionType: { arg: '', status: 'INCOMPLETE', message: '' },
          optionValue: { arg: '', status: 'INCOMPLETE', message: '' }
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
          optionType: { arg: '', status: 'INCOMPLETE', message: '' },
          optionValue: { arg: '', status: 'INCOMPLETE', message: '' }
        }
      }
    },
    {
      name: 'ts|v',
      setup: function() {
        helpers.setInput(options, 'tsv ', 2);
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
          optionType: { arg: '', status: 'INCOMPLETE', message: '' },
          optionValue: { arg: '', status: 'INCOMPLETE', message: '' }
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
            message: ''
          },
          optionValue: {
            value: undefined,
            arg: '',
            status: 'INCOMPLETE',
            message: ''
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
            message: ''
          },
          optionValue: {
            value: undefined,
            arg: '',
            status: 'INCOMPLETE',
            message: ''
          }
        }
      }
    },
    {
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
            message: ''
          },
          optionValue: {
            value: undefined,
            arg: '',
            status: 'INCOMPLETE',
            message: ''
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
        tooltipState: 'true:isError',
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
            message: ''
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
            value: mockCommands.option1,
            arg: ' option1',
            status: 'VALID',
            message: ''
          },
          optionValue: {
            value: undefined,
            arg: '',
            status: 'INCOMPLETE',
            message: ''
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
            value: mockCommands.option1,
            arg: ' option1 ',
            status: 'VALID',
            message: ''
          },
          optionValue: {
            value: undefined,
            arg: '',
            status: 'INCOMPLETE',
            message: ''
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
            value: mockCommands.option2,
            arg: ' option2',
            status: 'VALID',
            message: ''
          },
          optionValue: {
            value: undefined,
            arg: '',
            status: 'INCOMPLETE',
            message: ''
          }
        }
      }
    },
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
            value: mockCommands.option1,
            arg: ' option1',
            status: 'VALID',
            message: ''
          },
          optionValue: {
            value: '6',
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
            value: mockCommands.option2,
            arg: ' option2',
            status: 'VALID',
            message: ''
          },
          optionValue: {
            value: 6,
            arg: ' 6',
            status: 'VALID',
            message: ''
          }
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

exports.testSingleString = function(options) {
  return helpers.audit(options, [
    {
      setup:    'tsr',
      check: {
        input:  'tsr',
        hints:     ' <text>',
        markup: 'VVV',
        cursor: 3,
        current: '__command',
        status: 'ERROR',
        unassigned: [ ],
        args: {
          command: { name: 'tsr' },
          text: {
            value: undefined,
            arg: '',
            status: 'INCOMPLETE',
            message: ''
          }
        }
      }
    },
    {
      setup:    'tsr ',
      check: {
        input:  'tsr ',
        hints:      '<text>',
        markup: 'VVVV',
        cursor: 4,
        current: 'text',
        status: 'ERROR',
        predictions: [ ],
        unassigned: [ ],
        args: {
          command: { name: 'tsr' },
          text: {
            value: undefined,
            arg: '',
            status: 'INCOMPLETE',
            message: ''
          }
        }
      }
    },
    {
      setup:    'tsr h',
      check: {
        input:  'tsr h',
        hints:       '',
        markup: 'VVVVV',
        cursor: 5,
        current: 'text',
        status: 'VALID',
        predictions: [ ],
        unassigned: [ ],
        args: {
          command: { name: 'tsr' },
          text: {
            value: 'h',
            arg: ' h',
            status: 'VALID',
            message: ''
          }
        }
      }
    },
    {
      setup:    'tsr "h h"',
      check: {
        input:  'tsr "h h"',
        hints:           '',
        markup: 'VVVVVVVVV',
        cursor: 9,
        current: 'text',
        status: 'VALID',
        predictions: [ ],
        unassigned: [ ],
        args: {
          command: { name: 'tsr' },
          text: {
            value: 'h h',
            arg: ' "h h"',
            status: 'VALID',
            message: ''
          }
        }
      }
    },
    {
      setup:    'tsr h h h',
      check: {
        input:  'tsr h h h',
        hints:           '',
        markup: 'VVVVVVVVV',
        cursor: 9,
        current: 'text',
        status: 'VALID',
        predictions: [ ],
        unassigned: [ ],
        args: {
          command: { name: 'tsr' },
          text: {
            value: 'h h h',
            arg: ' h h h',
            status: 'VALID',
            message: ''
          }
        }
      }
    }
  ]);
};

exports.testSingleNumber = function(options) {
  return helpers.audit(options, [
    {
      setup:    'tsu',
      check: {
        input:  'tsu',
        hints:     ' <num>',
        markup: 'VVV',
        cursor: 3,
        current: '__command',
        status: 'ERROR',
        predictions: [ ],
        unassigned: [ ],
        args: {
          command: { name: 'tsu' },
          num: {
            value: undefined,
            arg: '',
            status: 'INCOMPLETE',
            message: ''
          }
        }
      }
    },
    {
      setup:    'tsu ',
      check: {
        input:  'tsu ',
        hints:      '<num>',
        markup: 'VVVV',
        cursor: 4,
        current: 'num',
        status: 'ERROR',
        predictions: [ ],
        unassigned: [ ],
        args: {
          command: { name: 'tsu' },
          num: {
            value: undefined,
            arg: '',
            status: 'INCOMPLETE',
            message: ''
          }
        }
      }
    },
    {
      setup:    'tsu 1',
      check: {
        input:  'tsu 1',
        hints:       '',
        markup: 'VVVVV',
        cursor: 5,
        current: 'num',
        status: 'VALID',
        predictions: [ ],
        unassigned: [ ],
        args: {
          command: { name: 'tsu' },
          num: { value: 1, arg: ' 1', status: 'VALID', message: '' }
        }
      }
    },
    {
      setup:    'tsu x',
      check: {
        input:  'tsu x',
        hints:       '',
        markup: 'VVVVE',
        cursor: 5,
        current: 'num',
        status: 'ERROR',
        predictions: [ ],
        unassigned: [ ],
        tooltipState: 'true:isError',
        args: {
          command: { name: 'tsu' },
          num: {
            value: undefined,
            arg: ' x',
            status: 'ERROR',
            message: 'Can\'t convert "x" to a number.'
          }
        }
      }
    },
    {
      setup:    'tsu 1.5',
      check: {
        input:  'tsu 1.5',
        hints:       '',
        markup: 'VVVVEEE',
        cursor: 7,
        current: 'num',
        status: 'ERROR',
        predictions: [ ],
        unassigned: [ ],
        args: {
          command: { name: 'tsu' },
          num: {
            value: undefined,
            arg: ' 1.5',
            status: 'ERROR',
            message: 'Can\'t convert "1.5" to an integer.'
          }
        }
      }
    }
  ]);
};

exports.testSingleFloat = function(options) {
  return helpers.audit(options, [
    {
      setup:    'tsf',
      check: {
        input:  'tsf',
        hints:     ' <num>',
        markup: 'VVV',
        cursor: 3,
        current: '__command',
        status: 'ERROR',
        error: '',
        unassigned: [ ],
        args: {
          command: { name: 'tsf' },
          num: {
            value: undefined,
            arg: '',
            status: 'INCOMPLETE',
            message: ''
          }
        }
      }
    },
    {
      setup:    'tsf 1',
      check: {
        input:  'tsf 1',
        hints:       '',
        markup: 'VVVVV',
        cursor: 5,
        current: 'num',
        status: 'VALID',
        error: '',
        predictions: [ ],
        unassigned: [ ],
        args: {
          command: { name: 'tsf' },
          num: { value: 1, arg: ' 1', status: 'VALID', message: '' }
        }
      }
    },
    {
      setup:    'tsf 1.',
      check: {
        input:  'tsf 1.',
        hints:        '',
        markup: 'VVVVVV',
        cursor: 6,
        current: 'num',
        status: 'VALID',
        error: '',
        predictions: [ ],
        unassigned: [ ],
        args: {
          command: { name: 'tsf' },
          num: { value: 1, arg: ' 1.', status: 'VALID', message: '' }
        }
      }
    },
    {
      setup:    'tsf 1.5',
      check: {
        input:  'tsf 1.5',
        hints:         '',
        markup: 'VVVVVVV',
        cursor: 7,
        current: 'num',
        status: 'VALID',
        error: '',
        predictions: [ ],
        unassigned: [ ],
        args: {
          command: { name: 'tsf' },
          num: { value: 1.5, arg: ' 1.5', status: 'VALID', message: '' }
        }
      }
    },
    {
      setup:    'tsf 1.5x',
      check: {
        input:  'tsf 1.5x',
        hints:          '',
        markup: 'VVVVVVVV',
        cursor: 8,
        current: 'num',
        status: 'VALID',
        error: '',
        predictions: [ ],
        unassigned: [ ],
        args: {
          command: { name: 'tsf' },
          num: { value: 1.5, arg: ' 1.5x', status: 'VALID', message: '' }
        }
      }
    },
    {
      name: 'tsf x (cursor=4)',
      setup: function() {
        return helpers.setInput(options, 'tsf x', 4);
      },
      check: {
        input:  'tsf x',
        hints:       '',
        markup: 'VVVVE',
        cursor: 4,
        current: 'num',
        status: 'ERROR',
        error: 'Can\'t convert "x" to a number.',
        predictions: [ ],
        unassigned: [ ],
        args: {
          command: { name: 'tsf' },
          num: {
            value: undefined,
            arg: ' x',
            status: 'ERROR',
            message: 'Can\'t convert "x" to a number.'
          }
        }
      }
    }
  ]);
};

exports.testElementDom = function(options) {
  return helpers.audit(options, [
    {
      skipIf: options.isJsdom,
      setup:    'tse :root',
      check: {
        input:  'tse :root',
        hints:           ' [options]',
        markup: 'VVVVVVVVV',
        cursor: 9,
        current: 'node',
        status: 'VALID',
        predictions: [ ],
        unassigned: [ ],
        args: {
          command: { name: 'tse' },
          node: {
            value: options.window.document.documentElement,
            arg: ' :root',
            status: 'VALID',
            message: ''
          },
          nodes: { arg: '', status: 'VALID', message: '' },
          nodes2: { arg: '', status: 'VALID', message: '' },
        }
      }
    }
  ]);
};

exports.testElementWeb = function(options) {
  var inputElement = options.window.document.getElementById('gcli-input');

  return helpers.audit(options, [
    {
      skipIf: function gcliInputElementExists() {
        return inputElement == null || options.isJsdom;
      },
      setup:    'tse #gcli-input',
      check: {
        input:  'tse #gcli-input',
        hints:                 ' [options]',
        markup: 'VVVVVVVVVVVVVVV',
        cursor: 15,
        current: 'node',
        status: 'VALID',
        predictions: [ ],
        unassigned: [ ],
        args: {
          command: { name: 'tse' },
          node: {
            value: inputElement,
            arg: ' #gcli-input',
            status: 'VALID',
            message: ''
          },
          nodes: { arg: '', status: 'VALID', message: '' },
          nodes2: { arg: '', status: 'VALID', message: '' },
        }
      }
    }
  ]);
};

exports.testElement = function(options) {
  return helpers.audit(options, [
    {
      setup:    'tse',
      check: {
        input:  'tse',
        hints:     ' <node> [options]',
        markup: 'VVV',
        cursor: 3,
        current: '__command',
        status: 'ERROR',
        predictions: [ 'tse', 'tselarr' ],
        unassigned: [ ],
        args: {
          command: { name: 'tse' },
          node: { value: undefined, arg: '', status: 'INCOMPLETE', message: '' },
          nodes: { arg: '', status: 'VALID', message: '' },
          nodes2: { arg: '', status: 'VALID', message: '' },
        }
      }
    },
    {
      skipIf: options.isJsdom,
      setup:    'tse #gcli-nomatch',
      check: {
        input:  'tse #gcli-nomatch',
        hints:                   ' [options]',
        markup: 'VVVVIIIIIIIIIIIII',
        cursor: 17,
        current: 'node',
        status: 'ERROR',
        predictions: [ ],
        unassigned: [ ],
        outputState: 'false:default',
        tooltipState: 'true:isError',
        args: {
          command: { name: 'tse' },
          node: {
            value: undefined,
            arg: ' #gcli-nomatch',
            // This is somewhat debatable because this input can't be corrected
            // simply by typing so it's and error rather than incomplete,
            // however without digging into the CSS engine we can't tell that
            // so we default to incomplete
            status: 'INCOMPLETE',
            message: 'No matches'
          },
          nodes: { arg: '', status: 'VALID', message: '' },
          nodes2: { arg: '', status: 'VALID', message: '' },
        }
      }
    },
    {
      setup:    'tse #',
      check: {
        input:  'tse #',
        hints:       ' [options]',
        markup: 'VVVVE',
        cursor: 5,
        current: 'node',
        status: 'ERROR',
        predictions: [ ],
        unassigned: [ ],
        tooltipState: 'true:isError',
        args: {
          command: { name: 'tse' },
          node: {
            value: undefined,
            arg: ' #',
            status: 'ERROR',
            message: 'Syntax error in CSS query'
          },
          nodes: { arg: '', status: 'VALID', message: '' },
          nodes2: { arg: '', status: 'VALID', message: '' },
        }
      }
    },
    {
      setup:    'tse .',
      check: {
        input:  'tse .',
        hints:       ' [options]',
        markup: 'VVVVE',
        cursor: 5,
        current: 'node',
        status: 'ERROR',
        predictions: [ ],
        unassigned: [ ],
        tooltipState: 'true:isError',
        args: {
          command: { name: 'tse' },
          node: {
            value: undefined,
            arg: ' .',
            status: 'ERROR',
            message: 'Syntax error in CSS query'
          },
          nodes: { arg: '', status: 'VALID', message: '' },
          nodes2: { arg: '', status: 'VALID', message: '' },
        }
      }
    },
    {
      skipIf: options.isJsdom,
      setup:    'tse *',
      check: {
        input:  'tse *',
        hints:       ' [options]',
        markup: 'VVVVE',
        cursor: 5,
        current: 'node',
        status: 'ERROR',
        predictions: [ ],
        unassigned: [ ],
        tooltipState: 'true:isError',
        args: {
          command: { name: 'tse' },
          node: {
            value: undefined,
            arg: ' *',
            status: 'ERROR',
            message: /^Too many matches \([0-9]*\)/
          },
          nodes: { arg: '', status: 'VALID', message: '' },
          nodes2: { arg: '', status: 'VALID', message: '' },
        }
      }
    }
  ]);
};

exports.testNestedCommand = function(options) {
  return helpers.audit(options, [
    {
      setup:    'tsn',
      check: {
        input:  'tsn',
        hints:     '',
        markup: 'III',
        cursor: 3,
        current: '__command',
        status: 'ERROR',
        predictionsInclude: [
          'tsn deep', 'tsn deep down', 'tsn deep down nested',
          'tsn deep down nested cmd', 'tsn dif'
        ],
        unassigned: [ ],
        args: {
          command: { name: 'tsn' }
        }
      }
    },
    {
      setup:    'tsn ',
      check: {
        input:  'tsn ',
        hints:      '',
        markup: 'IIIV',
        cursor: 4,
        current: '__command',
        status: 'ERROR',
        unassigned: [ ]
      }
    },
    {
      skipIf: options.isPhantomjs,
      setup:    'tsn x',
      check: {
        input:  'tsn x',
        hints:       ' -> tsn ext',
        markup: 'IIIVI',
        cursor: 5,
        current: '__command',
        status: 'ERROR',
        predictions: [ 'tsn ext' ],
        unassigned: [ ]
      }
    },
    {
      setup:    'tsn dif',
      check: {
        input:  'tsn dif',
        hints:         ' <text>',
        markup: 'VVVVVVV',
        cursor: 7,
        current: '__command',
        status: 'ERROR',
        predictions: [ ],
        unassigned: [ ],
        args: {
          command: { name: 'tsn dif' },
          text: {
            value: undefined,
            arg: '',
            status: 'INCOMPLETE',
            message: ''
          }
        }
      }
    },
    {
      setup:    'tsn dif ',
      check: {
        input:  'tsn dif ',
        hints:          '<text>',
        markup: 'VVVVVVVV',
        cursor: 8,
        current: 'text',
        status: 'ERROR',
        predictions: [ ],
        unassigned: [ ],
        args: {
          command: { name: 'tsn dif' },
          text: {
            value: undefined,
            arg: '',
            status: 'INCOMPLETE',
            message: ''
          }
        }
      }
    },
    {
      setup:    'tsn dif x',
      check: {
        input:  'tsn dif x',
        hints:           '',
        markup: 'VVVVVVVVV',
        cursor: 9,
        current: 'text',
        status: 'VALID',
        predictions: [ ],
        unassigned: [ ],
        args: {
          command: { name: 'tsn dif' },
          text: { value: 'x', arg: ' x', status: 'VALID', message: '' }
        }
      }
    },
    {
      setup:    'tsn ext',
      check: {
        input:  'tsn ext',
        hints:         ' <text>',
        markup: 'VVVVVVV',
        cursor: 7,
        current: '__command',
        status: 'ERROR',
        predictions: [ 'tsn ext', 'tsn exte', 'tsn exten', 'tsn extend' ],
        unassigned: [ ],
        args: {
          command: { name: 'tsn ext' },
          text: {
            value: undefined,
            arg: '',
            status: 'INCOMPLETE',
            message: ''
          }
        }
      }
    },
    {
      setup:    'tsn exte',
      check: {
        input:  'tsn exte',
        hints:          ' <text>',
        markup: 'VVVVVVVV',
        cursor: 8,
        current: '__command',
        status: 'ERROR',
        predictions: [ 'tsn exte', 'tsn exten', 'tsn extend' ],
        unassigned: [ ],
        args: {
          command: { name: 'tsn exte' },
          text: {
            value: undefined,
            arg: '',
            status: 'INCOMPLETE',
            message: ''
          }
        }
      }
    },
    {
      setup:    'tsn exten',
      check: {
        input:  'tsn exten',
        hints:           ' <text>',
        markup: 'VVVVVVVVV',
        cursor: 9,
        current: '__command',
        status: 'ERROR',
        predictions: [ 'tsn exten', 'tsn extend' ],
        unassigned: [ ],
        args: {
          command: { name: 'tsn exten' },
          text: {
            value: undefined,
            arg: '',
            status: 'INCOMPLETE',
            message: ''
          }
        }
      }
    },
    {
      setup:    'tsn extend',
      check: {
        input:  'tsn extend',
        hints:            ' <text>',
        markup: 'VVVVVVVVVV',
        cursor: 10,
        current: '__command',
        status: 'ERROR',
        predictions: [ ],
        unassigned: [ ],
        args: {
          command: { name: 'tsn extend' },
          text: {
            value: undefined,
            arg: '',
            status: 'INCOMPLETE',
            message: ''
          }
        }
      }
    },
    {
      setup:    'ts ',
      check: {
        input:  'ts ',
        hints:     '',
        markup: 'EEV',
        cursor: 3,
        current: '__command',
        status: 'ERROR',
        predictions: [ ],
        unassigned: [ ],
        tooltipState: 'true:isError'
      }
    },
  ]);
};

// From Bug 664203
exports.testDeeplyNested = function(options) {
  return helpers.audit(options, [
    {
      setup:    'tsn deep down nested',
      check: {
        input:  'tsn deep down nested',
        hints:                      '',
        markup: 'IIIVIIIIVIIIIVIIIIII',
        cursor: 20,
        current: '__command',
        status: 'ERROR',
        predictions: [ 'tsn deep down nested', 'tsn deep down nested cmd' ],
        unassigned: [ ],
        outputState: 'false:default',
        tooltipState: 'false:default',
        args: {
          command: { name: 'tsn deep down nested' },
        }
      }
    },
    {
      setup:    'tsn deep down nested cmd',
      check: {
        input:  'tsn deep down nested cmd',
        hints:                          '',
        markup: 'VVVVVVVVVVVVVVVVVVVVVVVV',
        cursor: 24,
        current: '__command',
        status: 'VALID',
        predictions: [ ],
        unassigned: [ ],
        args: {
          command: { name: 'tsn deep down nested cmd' },
        }
      }
    }
  ]);
};


// });
