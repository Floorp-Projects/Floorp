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

var TEST_URI = "data:text/html;charset=utf-8,<p id='gcli-input'>gcli-testExec.js</p>";

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
var nodetype = require('gcli/types/node');

var mockBody = {
  style: {}
};

var mockEmptyNodeList = {
  length: 0,
  item: function() { return null; }
};

var mockRootNodeList = {
  length: 1,
  item: function(i) { return mockBody; }
};

var mockDoc = {
  querySelectorAll: function(css) {
    return (css === ':root') ? mockRootNodeList : mockEmptyNodeList;
  }
};

exports.testParamGroup = function(options) {
  var tsg = options.requisition.system.commands.get('tsg');

  assert.is(tsg.params[0].groupName, null, 'tsg param 0 group null');
  assert.is(tsg.params[1].groupName, 'First', 'tsg param 1 group First');
  assert.is(tsg.params[2].groupName, 'First', 'tsg param 2 group First');
  assert.is(tsg.params[3].groupName, 'Second', 'tsg param 3 group Second');
  assert.is(tsg.params[4].groupName, 'Second', 'tsg param 4 group Second');
};

exports.testWithHelpers = function(options) {
  return helpers.audit(options, [
    {
      setup:    'tss',
      check: {
        input:  'tss',
        hints:     '',
        markup: 'VVV',
        cursor: 3,
        current: '__command',
        status: 'VALID',
        unassigned: [ ],
        args: {
          command: { name: 'tss' },
        }
      },
      exec: {
        output: /^Exec: tss/,
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
            value: 'string',
            arg: ' option1',
            status: 'VALID',
            message: ''
          },
          optionValue: {
            arg: ' 10',
            status: 'VALID',
            message: ''
          }
        }
      },
      exec: {
        output: 'Exec: tsv optionType=string, optionValue=10'
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
            value: 'number',
            arg: ' option2',
            status: 'VALID',
            message: ''
          },
          optionValue: {
            arg: ' 10',
            status: 'VALID',
            message: ''
          }
        }
      },
      exec: {
        output: 'Exec: tsv optionType=number, optionValue=10'
      }
    },
    // Delegated remote types can't transfer value types so we only test for
    // the value of optionValue when we're local
    {
      skipIf: options.isRemote,
      setup: 'tsv option1 10',
      check: {
        args: { optionValue: { value: '10' } }
      },
      exec: {
        output: 'Exec: tsv optionType=string, optionValue=10'
      }
    },
    {
      skipIf: options.isRemote,
      setup: 'tsv option2 10',
      check: {
        args: { optionValue: { value: 10 } }
      },
      exec: {
        output: 'Exec: tsv optionType=number, optionValue=10'
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
        output: 'Exec: tsr text=fred'
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
        output: 'Exec: tsr text=fred bloggs'
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
        output: 'Exec: tsr text=fred bloggs'
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
        output: 'Exec: tsr text=fred bloggs'
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
        output: 'Exec: tsb toggle=false'
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
        output: 'Exec: tsb toggle=true'
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
          num: { value: 10, arg: ' 10', status: 'VALID', message: '' }
        }
      },
      exec: {
        output: 'Exec: tsu num=10'
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
          num: { value: 10, arg: ' --num 10', status: 'VALID', message: '' }
        }
      },
      exec: {
        output: 'Exec: tsu num=10'
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
        output: 'Exec: tsj javascript=1 + 1'
      }
    }
  ]);
};

exports.testExecNode = function(options) {
  var origDoc = nodetype.getDocument();
  nodetype.setDocument(mockDoc);

  return helpers.audit(options, [
    {
      skipIf: options.isNoDom,
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
            value: mockBody,
            arg: ' :root',
            status: 'VALID',
            message: ''
          },
          nodes: {
            value: mockEmptyNodeList,
            arg: '',
            status: 'VALID',
            message: ''
          },
          nodes2: {
            value: mockEmptyNodeList,
            arg: '',
            status: 'VALID',
            message: ''
          }
        }
      },
      exec: {
        output: /^Exec: tse/
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
          text: { value: 'fred', arg: ' fred', status: 'VALID', message: '' }
        }
      },
      exec: {
        output: 'Exec: tsnDif text=fred'
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
        output: 'Exec: tsnExten text=fred'
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
        output: 'Exec: tsnExtend text=fred'
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
        output: 'Exec: tselarr num=1, arr='
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
        output: 'Exec: tselarr num=1, arr=a'
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
        output: 'Exec: tselarr num=1, arr=a,b'
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
        output: 'Exec: tsm abc=a, txt=10, num=10'
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
        output: 'Exec: tsg solo=aaa, txt1=null, bool=false, txt2=d, num=42'
      }
    }
  ]);

};
