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

const { classes: Cc, interfaces: Ci, utils: Cu } = Components;
let { require: require, define: define } = Cu.import("resource://gre/modules/devtools/Require.jsm", {});
Cu.import("resource:///modules/devtools/gcli.jsm", {});

// <INJECTED SOURCE:END>

var mockCommands = {};

var util = require('util/util');
var canon = require('gcli/canon');

var types = require('gcli/types');
var SelectionType = require('gcli/types/selection').SelectionType;
var DelegateType = require('gcli/types/basic').DelegateType;


/**
 * Registration and de-registration.
 */
mockCommands.setup = function(opts) {
  // setup/shutdown needs to register/unregister types, however that means we
  // need to re-initialize mockCommands.option1 and mockCommands.option2 with
  // the actual types
  mockCommands.option1.type = types.getType('string');
  mockCommands.option2.type = types.getType('number');

  types.registerType(mockCommands.optionType);
  types.registerType(mockCommands.optionValue);

  canon.addCommand(mockCommands.tsv);
  canon.addCommand(mockCommands.tsr);
  canon.addCommand(mockCommands.tso);
  canon.addCommand(mockCommands.tse);
  canon.addCommand(mockCommands.tsj);
  canon.addCommand(mockCommands.tsb);
  canon.addCommand(mockCommands.tss);
  canon.addCommand(mockCommands.tsu);
  canon.addCommand(mockCommands.tsf);
  canon.addCommand(mockCommands.tsn);
  canon.addCommand(mockCommands.tsnDif);
  canon.addCommand(mockCommands.tsnExt);
  canon.addCommand(mockCommands.tsnExte);
  canon.addCommand(mockCommands.tsnExten);
  canon.addCommand(mockCommands.tsnExtend);
  canon.addCommand(mockCommands.tsnDeep);
  canon.addCommand(mockCommands.tsnDeepDown);
  canon.addCommand(mockCommands.tsnDeepDownNested);
  canon.addCommand(mockCommands.tsnDeepDownNestedCmd);
  canon.addCommand(mockCommands.tselarr);
  canon.addCommand(mockCommands.tsm);
  canon.addCommand(mockCommands.tsg);
  canon.addCommand(mockCommands.tshidden);
  canon.addCommand(mockCommands.tscook);
  canon.addCommand(mockCommands.tslong);
};

mockCommands.shutdown = function(opts) {
  canon.removeCommand(mockCommands.tsv);
  canon.removeCommand(mockCommands.tsr);
  canon.removeCommand(mockCommands.tso);
  canon.removeCommand(mockCommands.tse);
  canon.removeCommand(mockCommands.tsj);
  canon.removeCommand(mockCommands.tsb);
  canon.removeCommand(mockCommands.tss);
  canon.removeCommand(mockCommands.tsu);
  canon.removeCommand(mockCommands.tsf);
  canon.removeCommand(mockCommands.tsn);
  canon.removeCommand(mockCommands.tsnDif);
  canon.removeCommand(mockCommands.tsnExt);
  canon.removeCommand(mockCommands.tsnExte);
  canon.removeCommand(mockCommands.tsnExten);
  canon.removeCommand(mockCommands.tsnExtend);
  canon.removeCommand(mockCommands.tsnDeep);
  canon.removeCommand(mockCommands.tsnDeepDown);
  canon.removeCommand(mockCommands.tsnDeepDownNested);
  canon.removeCommand(mockCommands.tsnDeepDownNestedCmd);
  canon.removeCommand(mockCommands.tselarr);
  canon.removeCommand(mockCommands.tsm);
  canon.removeCommand(mockCommands.tsg);
  canon.removeCommand(mockCommands.tshidden);
  canon.removeCommand(mockCommands.tscook);
  canon.removeCommand(mockCommands.tslong);

  types.deregisterType(mockCommands.optionType);
  types.deregisterType(mockCommands.optionValue);
};


mockCommands.option1 = { type: types.getType('string') };
mockCommands.option2 = { type: types.getType('number') };

var lastOption = undefined;
var debug = false;

mockCommands.optionType = new SelectionType({
  name: 'optionType',
  lookup: [
    { name: 'option1', value: mockCommands.option1 },
    { name: 'option2', value: mockCommands.option2 }
  ],
  noMatch: function() {
    lastOption = undefined;
    if (debug) {
      console.log('optionType.noMatch: lastOption = undefined');
    }
  },
  stringify: function(option) {
    lastOption = option;
    if (debug) {
      console.log('optionType.stringify: lastOption = ', lastOption);
    }
    return SelectionType.prototype.stringify.call(this, option);
  },
  parse: function(arg) {
    var promise = SelectionType.prototype.parse.call(this, arg);
    promise.then(function(conversion) {
      lastOption = conversion.value;
      if (debug) {
        console.log('optionType.parse: lastOption = ', lastOption);
      }
    });
    return promise;
  }
});

mockCommands.optionValue = new DelegateType({
  name: 'optionValue',
  delegateType: function() {
    if (lastOption && lastOption.type) {
      return lastOption.type;
    }
    else {
      return types.getType('blank');
    }
  }
});

mockCommands.onCommandExec = util.createEvent('commands.onCommandExec');

function createExec(name) {
  return function(args, context) {
    var data = {
      command: mockCommands[name],
      args: args,
      context: context
    };
    mockCommands.onCommandExec(data);
    var argsOut = Object.keys(args).map(function(key) {
      return key + '=' + args[key];
    }).join(', ');
    return 'Exec: ' + name + ' ' + argsOut;
  };
}

mockCommands.tsv = {
  name: 'tsv',
  params: [
    { name: 'optionType', type: 'optionType' },
    { name: 'optionValue', type: 'optionValue' }
  ],
  exec: createExec('tsv')
};

mockCommands.tsr = {
  name: 'tsr',
  params: [ { name: 'text', type: 'string' } ],
  exec: createExec('tsr')
};

mockCommands.tso = {
  name: 'tso',
  params: [ { name: 'text', type: 'string', defaultValue: null } ],
  exec: createExec('tso')
};

mockCommands.tse = {
  name: 'tse',
  params: [
    { name: 'node', type: 'node' },
    {
      group: 'options',
      params: [
        { name: 'nodes', type: { name: 'nodelist' } },
        { name: 'nodes2', type: { name: 'nodelist', allowEmpty: true } }
      ]
    }
  ],
  exec: createExec('tse')
};

mockCommands.tsj = {
  name: 'tsj',
  params: [ { name: 'javascript', type: 'javascript' } ],
  exec: createExec('tsj')
};

mockCommands.tsb = {
  name: 'tsb',
  params: [ { name: 'toggle', type: 'boolean' } ],
  exec: createExec('tsb')
};

mockCommands.tss = {
  name: 'tss',
  exec: createExec('tss')
};

mockCommands.tsu = {
  name: 'tsu',
  params: [ { name: 'num', type: { name: 'number', max: 10, min: -5, step: 3 } } ],
  exec: createExec('tsu')
};

mockCommands.tsf = {
  name: 'tsf',
  params: [ { name: 'num', type: { name: 'number', allowFloat: true, max: 11.5, min: -6.5, step: 1.5 } } ],
  exec: createExec('tsf')
};

mockCommands.tsn = {
  name: 'tsn'
};

mockCommands.tsnDif = {
  name: 'tsn dif',
  description: 'tsn dif',
  params: [ { name: 'text', type: 'string', description: 'tsn dif text' } ],
  exec: createExec('tsnDif')
};

mockCommands.tsnExt = {
  name: 'tsn ext',
  params: [ { name: 'text', type: 'string' } ],
  exec: createExec('tsnExt')
};

mockCommands.tsnExte = {
  name: 'tsn exte',
  params: [ { name: 'text', type: 'string' } ],
  exec: createExec('tsnExte')
};

mockCommands.tsnExten = {
  name: 'tsn exten',
  params: [ { name: 'text', type: 'string' } ],
  exec: createExec('tsnExten')
};

mockCommands.tsnExtend = {
  name: 'tsn extend',
  params: [ { name: 'text', type: 'string' } ],
  exec: createExec('tsnExtend')
};

mockCommands.tsnDeep = {
  name: 'tsn deep'
};

mockCommands.tsnDeepDown = {
  name: 'tsn deep down'
};

mockCommands.tsnDeepDownNested = {
  name: 'tsn deep down nested'
};

mockCommands.tsnDeepDownNestedCmd = {
  name: 'tsn deep down nested cmd',
  exec: createExec('tsnDeepDownNestedCmd')
};

mockCommands.tshidden = {
  name: 'tshidden',
  hidden: true,
  params: [
    {
      group: 'Options',
      params: [
        {
          name: 'visible',
          type: 'string',
          defaultValue: null,
          description: 'visible'
        },
        {
          name: 'invisiblestring',
          type: 'string',
          description: 'invisiblestring',
          defaultValue: null,
          hidden: true
        },
        {
          name: 'invisibleboolean',
          type: 'boolean',
          description: 'invisibleboolean',
          hidden: true
        }
      ]
    }
  ],
  exec: createExec('tshidden')
};

mockCommands.tselarr = {
  name: 'tselarr',
  params: [
    { name: 'num', type: { name: 'selection', data: [ '1', '2', '3' ] } },
    { name: 'arr', type: { name: 'array', subtype: 'string' } }
  ],
  exec: createExec('tselarr')
};

mockCommands.tsm = {
  name: 'tsm',
  description: 'a 3-param test selection|string|number',
  params: [
    { name: 'abc', type: { name: 'selection', data: [ 'a', 'b', 'c' ] } },
    { name: 'txt', type: 'string' },
    { name: 'num', type: { name: 'number', max: 42, min: 0 } }
  ],
  exec: createExec('tsm')
};

mockCommands.tsg = {
  name: 'tsg',
  description: 'a param group test',
  params: [
    {
      name: 'solo',
      type: { name: 'selection', data: [ 'aaa', 'bbb', 'ccc' ] },
      description: 'solo param'
    },
    {
      group: 'First',
      params: [
        {
          name: 'txt1',
          type: 'string',
          defaultValue: null,
          description: 'txt1 param'
        },
        {
          name: 'bool',
          type: 'boolean',
          description: 'bool param'
        }
      ]
    },
    {
      group: 'Second',
      params: [
        {
          name: 'txt2',
          type: 'string',
          defaultValue: 'd',
          description: 'txt2 param'
        },
        {
          name: 'num',
          type: { name: 'number', min: 40 },
          defaultValue: 42,
          description: 'num param'
        }
      ]
    }
  ],
  exec: createExec('tsg')
};

mockCommands.tscook = {
  name: 'tscook',
  description: 'param group test to catch problems with cookie command',
  params: [
    {
      name: 'key',
      type: 'string',
      description: 'tscookKeyDesc'
    },
    {
      name: 'value',
      type: 'string',
      description: 'tscookValueDesc'
    },
    {
      group: 'tscookOptionsDesc',
      params: [
        {
          name: 'path',
          type: 'string',
          defaultValue: '/',
          description: 'tscookPathDesc'
        },
        {
          name: 'domain',
          type: 'string',
          defaultValue: null,
          description: 'tscookDomainDesc'
        },
        {
          name: 'secure',
          type: 'boolean',
          description: 'tscookSecureDesc'
        }
      ]
    }
  ],
  exec: createExec('tscook')
};

mockCommands.tslong = {
  name: 'tslong',
  description: 'long param tests to catch problems with the jsb command',
  returnValue:'string',
  params: [
    {
      name: 'msg',
      type: 'string',
      description: 'msg Desc'
    },
    {
      group: "Options Desc",
      params: [
        {
          name: 'num',
          type: 'number',
          description: 'num Desc',
          defaultValue: 2
        },
        {
          name: 'sel',
          type: {
            name: 'selection',
            lookup: [
              { name: "space", value: " " },
              { name: "tab", value: "\t" }
            ]
          },
          description: 'sel Desc',
          defaultValue: ' '
        },
        {
          name: 'bool',
          type: 'boolean',
          description: 'bool Desc'
        },
        {
          name: 'num2',
          type: 'number',
          description: 'num2 Desc',
          defaultValue: -1
        },
        {
          name: 'bool2',
          type: 'boolean',
          description: 'bool2 Desc'
        },
        {
          name: 'sel2',
          type: {
            name: 'selection',
            data: [ 'collapse', 'basic', 'with space', 'with two spaces' ]
          },
          description: 'sel2 Desc',
          defaultValue: "collapse"
        }
      ]
    }
  ],
  exec: createExec('tslong')
};


// });
