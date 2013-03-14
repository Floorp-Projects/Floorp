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

const TEST_URI = "data:text/html;charset=utf-8,<p id='gcli-input'>gcli-testExec.js</p>";

function test() {
  helpers.addTabWithToolbar(TEST_URI, function(options) {
    return helpers.runTests(options, exports);
  }).then(finish);
}

// <INJECTED SOURCE:END>

'use strict';

// var mockCommands = require('gclitest/mockCommands');
var nodetype = require('gcli/types/node');
// var helpers = require('gclitest/helpers');


exports.setup = function(options) {
  mockCommands.setup();
};

exports.shutdown = function(options) {
  mockCommands.shutdown();
};

var mockBody = {
  style: {}
};

var mockDoc = {
  querySelectorAll: function(css) {
    if (css === ':root') {
      return {
        length: 1,
        item: function(i) {
          return mockBody;
        }
      };
    }
    else {
      return {
        length: 0,
        item: function() { return null; }
      };
    }
  }
};

exports.testWithHelpers = function(options) {
  return helpers.audit(options, [
    {
      skipIf: options.isJsdom,
      setup:    'tss',
      check: {
        input:  'tss',
        hints:     '',
        markup: 'VVV',
        cursor: 3,
        current: '__command',
        status: 'VALID',
        predictions: [ ],
        unassigned: [ ],
        args: {
          command: { name: 'tss' },
        }
      },
      exec: {
        output: 'Exec: tss',
        completed: true,
      }
    },
    {
      setup:    'tsv option1 10',
      check: {
        input:  'tsv option1 10',
        hints:                '',
        markup: 'VVVVVVVVVVVVVV',
        cursor: 14,
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
            value: '10',
            arg: ' 10',
            status: 'VALID',
            message: ''
          }
        }
      },
      exec: {
        output: 'Exec: tsv optionType=[object Object], optionValue=10',
        completed: true
      }
    },
    {
      setup:    'tsv option2 10',
      check: {
        input:  'tsv option2 10',
        hints:                '',
        markup: 'VVVVVVVVVVVVVV',
        cursor: 14,
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
            value: 10,
            arg: ' 10',
            status: 'VALID',
            message: ''
          }
        }
      },
      exec: {
        output: 'Exec: tsv optionType=[object Object], optionValue=10',
        completed: true
      }
    }
  ]);
};

exports.testExecText = function(options) {
  return helpers.audit(options, [
    {
      setup:    'tsr fred',
      check: {
        input:  'tsr fred',
        hints:          '',
        markup: 'VVVVVVVV',
        cursor: 8,
        current: 'text',
        status: 'VALID',
        predictions: [ ],
        unassigned: [ ],
        args: {
          command: { name: 'tsr' },
          text: {
            value: 'fred',
            arg: ' fred',
            status: 'VALID',
            message: ''
          }
        }
      },
      exec: {
        output: 'Exec: tsr text=fred',
        completed: true,
      }
    },
    {
      setup:    'tsr fred bloggs',
      check: {
        input:  'tsr fred bloggs',
        hints:                 '',
        markup: 'VVVVVVVVVVVVVVV',
        cursor: 15,
        current: 'text',
        status: 'VALID',
        predictions: [ ],
        unassigned: [ ],
        args: {
          command: { name: 'tsr' },
          text: {
            value: 'fred bloggs',
            arg: ' fred bloggs',
            status: 'VALID',
            message: ''
          }
        }
      },
      exec: {
        output: 'Exec: tsr text=fred bloggs',
        completed: true,
      }
    },
    {
      setup:    'tsr "fred bloggs"',
      check: {
        input:  'tsr "fred bloggs"',
        hints:                   '',
        markup: 'VVVVVVVVVVVVVVVVV',
        cursor: 17,
        current: 'text',
        status: 'VALID',
        predictions: [ ],
        unassigned: [ ],
        args: {
          command: { name: 'tsr' },
          text: {
            value: 'fred bloggs',
            arg: ' "fred bloggs"',
            status: 'VALID',
            message: ''
          }
        }
      },
      exec: {
        output: 'Exec: tsr text=fred bloggs',
        completed: true,
      }
    },
    {
      setup:    'tsr "fred bloggs',
      check: {
        input:  'tsr "fred bloggs',
        hints:                  '',
        markup: 'VVVVVVVVVVVVVVVV',
        cursor: 16,
        current: 'text',
        status: 'VALID',
        predictions: [ ],
        unassigned: [ ],
        args: {
          command: { name: 'tsr' },
          text: {
            value: 'fred bloggs',
            arg: ' "fred bloggs',
            status: 'VALID',
            message: ''
          }
        }
      },
      exec: {
        output: 'Exec: tsr text=fred bloggs',
        completed: true,
      }
    }
  ]);
};

exports.testExecBoolean = function(options) {
  return helpers.audit(options, [
    {
      setup:    'tsb',
      check: {
        input:  'tsb',
        hints:     ' [toggle]',
        markup: 'VVV',
        cursor: 3,
        current: '__command',
        status: 'VALID',
        predictions: [ ],
        unassigned: [ ],
        args: {
          command: { name: 'tsb' },
          toggle: {
            value: false,
            arg: '',
            status: 'VALID',
            message: ''
          }
        }
      },
      exec: {
        output: 'Exec: tsb toggle=false',
        completed: true,
      }
    },
    {
      setup:    'tsb --toggle',
      check: {
        input:  'tsb --toggle',
        hints:              '',
        markup: 'VVVVVVVVVVVV',
        cursor: 12,
        current: 'toggle',
        status: 'VALID',
        predictions: [ ],
        unassigned: [ ],
        outputState: 'false:default',
        args: {
          command: { name: 'tsb' },
          toggle: {
            value: true,
            arg: ' --toggle',
            status: 'VALID',
            message: ''
          }
        }
      },
      exec: {
        output: 'Exec: tsb toggle=true',
        completed: true,
      }
    }
  ]);
};

exports.testExecNumber = function(options) {
  return helpers.audit(options, [
    {
      setup:    'tsu 10',
      check: {
        input:  'tsu 10',
        hints:        '',
        markup: 'VVVVVV',
        cursor: 6,
        current: 'num',
        status: 'VALID',
        predictions: [ ],
        unassigned: [ ],
        args: {
          command: { name: 'tsu' },
          num: { value: 10, arg: ' 10', status: 'VALID', message: '' },
        }
      },
      exec: {
        output: 'Exec: tsu num=10',
        completed: true,
      }
    },
    {
      setup:    'tsu --num 10',
      check: {
        input:  'tsu --num 10',
        hints:              '',
        markup: 'VVVVVVVVVVVV',
        cursor: 12,
        current: 'num',
        status: 'VALID',
        predictions: [ ],
        unassigned: [ ],
        args: {
          command: { name: 'tsu' },
          num: { value: 10, arg: ' --num 10', status: 'VALID', message: '' },
        }
      },
      exec: {
        output: 'Exec: tsu num=10',
        completed: true,
      }
    }
  ]);
};

exports.testExecScript = function(options) {
  return helpers.audit(options, [
    {
      // Bug 704829 - Enable GCLI Javascript parameters
      // The answer to this should be 2
      setup:    'tsj { 1 + 1 }',
      check: {
        input:  'tsj { 1 + 1 }',
        hints:               '',
        markup: 'VVVVVVVVVVVVV',
        cursor: 13,
        current: 'javascript',
        status: 'VALID',
        predictions: [ ],
        unassigned: [ ],
        args: {
          command: { name: 'tsj' },
          javascript: {
            value: '1 + 1',
            arg: ' { 1 + 1 }',
            status: 'VALID',
            message: ''
          }
        }
      },
      exec: {
        output: 'Exec: tsj javascript=1 + 1',
        completed: true,
      }
    }
  ]);
};

exports.testExecNode = function(options) {
  var origDoc = nodetype.getDocument();
  nodetype.setDocument(mockDoc);

  return helpers.audit(options, [
    {
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
          node: { value: mockBody, arg: ' :root', status: 'VALID', message: '' },
          nodes: { arg: '', status: 'VALID', message: '' },
          nodes2: { arg: '', status: 'VALID', message: '' },
        }
      },
      exec: {
        output: 'Exec: tse node=[object Object], nodes=[object Object], nodes2=[object Object]',
        completed: true,
      },
      post: function() {
        nodetype.setDocument(origDoc);
      }
    }
  ]);
};

exports.testExecSubCommand = function(options) {
  return helpers.audit(options, [
    {
      setup:    'tsn dif fred',
      check: {
        input:  'tsn dif fred',
        hints:              '',
        markup: 'VVVVVVVVVVVV',
        cursor: 12,
        current: 'text',
        status: 'VALID',
        predictions: [ ],
        unassigned: [ ],
        args: {
          command: { name: 'tsn dif' },
          text: { value: 'fred', arg: ' fred', status: 'VALID', message: '' },
        }
      },
      exec: {
        output: 'Exec: tsnDif text=fred',
        completed: true,
      }
    },
    {
      setup:    'tsn exten fred',
      check: {
        input:  'tsn exten fred',
        hints:                '',
        markup: 'VVVVVVVVVVVVVV',
        cursor: 14,
        current: 'text',
        status: 'VALID',
        predictions: [ ],
        unassigned: [ ],
        args: {
          command: { name: 'tsn exten' },
          text: { value: 'fred', arg: ' fred', status: 'VALID', message: '' },
        }
      },
      exec: {
        output: 'Exec: tsnExten text=fred',
        completed: true,
      }
    },
    {
      setup:    'tsn extend fred',
      check: {
        input:  'tsn extend fred',
        hints:                 '',
        markup: 'VVVVVVVVVVVVVVV',
        cursor: 15,
        current: 'text',
        status: 'VALID',
        predictions: [ ],
        unassigned: [ ],
        args: {
          command: { name: 'tsn extend' },
          text: { value: 'fred', arg: ' fred', status: 'VALID', message: '' },
        }
      },
      exec: {
        output: 'Exec: tsnExtend text=fred',
        completed: true,
      }
    }
  ]);
};

exports.testExecArray = function(options) {
  return helpers.audit(options, [
    {
      setup:    'tselarr 1',
      check: {
        input:  'tselarr 1',
        hints:           '',
        markup: 'VVVVVVVVV',
        cursor: 9,
        current: 'num',
        status: 'VALID',
        predictions: ['1'],
        unassigned: [ ],
        outputState: 'false:default',
        args: {
          command: { name: 'tselarr' },
          num: { value: '1', arg: ' 1', status: 'VALID', message: '' },
          arr: { /*value:,*/ arg: '{}', status: 'VALID', message: '' },
        }
      },
      exec: {
        output: 'Exec: tselarr num=1, arr=',
        completed: true,
      }
    },
    {
      setup:    'tselarr 1 a',
      check: {
        input:  'tselarr 1 a',
        hints:             '',
        markup: 'VVVVVVVVVVV',
        cursor: 11,
        current: 'arr',
        status: 'VALID',
        predictions: [ ],
        unassigned: [ ],
        args: {
          command: { name: 'tselarr' },
          num: { value: '1', arg: ' 1', status: 'VALID', message: '' },
          arr: { /*value:a,*/ arg: '{ a}', status: 'VALID', message: '' },
        }
      },
      exec: {
        output: 'Exec: tselarr num=1, arr=a',
        completed: true,
      }
    },
    {
      setup:    'tselarr 1 a b',
      check: {
        input:  'tselarr 1 a b',
        hints:               '',
        markup: 'VVVVVVVVVVVVV',
        cursor: 13,
        current: 'arr',
        status: 'VALID',
        predictions: [ ],
        unassigned: [ ],
        args: {
          command: { name: 'tselarr' },
          num: { value: '1', arg: ' 1', status: 'VALID', message: '' },
          arr: { /*value:a,b,*/ arg: '{ a, b}', status: 'VALID', message: '' },
        }
      },
      exec: {
        output: 'Exec: tselarr num=1, arr=a,b',
        completed: true,
      }
    }
  ]);
};

exports.testExecMultiple = function(options) {
  return helpers.audit(options, [
    {
      setup:    'tsm a 10 10',
      check: {
        input:  'tsm a 10 10',
        hints:             '',
        markup: 'VVVVVVVVVVV',
        cursor: 11,
        current: 'num',
        status: 'VALID',
        predictions: [ ],
        unassigned: [ ],
        args: {
          command: { name: 'tsm' },
          abc: { value: 'a', arg: ' a', status: 'VALID', message: '' },
          txt: { value: '10', arg: ' 10', status: 'VALID', message: '' },
          num: { value: 10, arg: ' 10', status: 'VALID', message: '' },
        }
      },
      exec: {
        output: 'Exec: tsm abc=a, txt=10, num=10',
        completed: true,
      }
    }
  ]);
};

exports.testExecDefaults = function(options) {
  return helpers.audit(options, [
    {
      // Bug 707009 - GCLI doesn't always fill in default parameters properly
      setup:    'tsg aaa',
      check: {
        input:  'tsg aaa',
        hints:         ' [options]',
        markup: 'VVVVVVV',
        cursor: 7,
        current: 'solo',
        status: 'VALID',
        predictions: ['aaa'],
        unassigned: [ ],
        args: {
          command: { name: 'tsg' },
          solo: { value: 'aaa', arg: ' aaa', status: 'VALID', message: '' },
          txt1: { value: undefined, arg: '', status: 'VALID', message: '' },
          bool: { value: false, arg: '', status: 'VALID', message: '' },
          txt2: { value: undefined, arg: '', status: 'VALID', message: '' },
          num: { value: undefined, arg: '', status: 'VALID', message: '' },
        }
      },
      exec: {
        output: 'Exec: tsg solo=aaa, txt1=null, bool=false, txt2=d, num=42',
        completed: true,
      }
    }
  ]);

};

// });
