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
Cu.import("resource://gre/modules/devtools/gcli.jsm", {});

// <INJECTED SOURCE:END>

var mockCommands = {};

// We use an alias for exports here because this module is used in Firefox
// mochitests where we don't have define/require

'use strict';

var util = require('util/util');
var canon = require('gcli/canon');
var types = require('gcli/types');

mockCommands.option1 = { };
mockCommands.option2 = { };
mockCommands.option3 = { };

mockCommands.optionType = {
  name: 'optionType',
  parent: 'selection',
  lookup: [
    { name: 'option1', value: mockCommands.option1 },
    { name: 'option2', value: mockCommands.option2 },
    { name: 'option3', value: mockCommands.option3 }
  ]
};

mockCommands.optionValue = {
  name: 'optionValue',
  parent: 'delegate',
  delegateType: function(executionContext) {
    if (executionContext != null) {
      var option = executionContext.getArgsObject().optionType;
      if (option != null) {
        return option.type;
      }
    }
    return types.createType('blank');
  }
};

mockCommands.onCommandExec = util.createEvent('commands.onCommandExec');

function createExec(name) {
  return function(args, executionContext) {
    var data = {
      args: args,
      context: executionContext
    };
    mockCommands.onCommandExec(data);
    var argsOut = Object.keys(args).map(function(key) {
      return key + '=' + args[key];
    }).join(', ');
    return 'Exec: ' + name + ' ' + argsOut;
  };
}

var tsv = {
  name: 'tsv',
  params: [
    { name: 'optionType', type: 'optionType' },
    { name: 'optionValue', type: 'optionValue' }
  ],
  exec: createExec('tsv')
};

var tsr = {
  name: 'tsr',
  params: [ { name: 'text', type: 'string' } ],
  exec: createExec('tsr')
};

var tsrsrsr = {
  name: 'tsrsrsr',
  params: [
    { name: 'p1', type: 'string' },
    { name: 'p2', type: 'string' },
    { name: 'p3', type: { name: 'string', allowBlank: true} },
  ],
  exec: createExec('tsrsrsr')
};

var tso = {
  name: 'tso',
  params: [ { name: 'text', type: 'string', defaultValue: null } ],
  exec: createExec('tso')
};

var tse = {
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

var tsj = {
  name: 'tsj',
  params: [ { name: 'javascript', type: 'javascript' } ],
  exec: createExec('tsj')
};

var tsb = {
  name: 'tsb',
  params: [ { name: 'toggle', type: 'boolean' } ],
  exec: createExec('tsb')
};

var tss = {
  name: 'tss',
  exec: createExec('tss')
};

var tsu = {
  name: 'tsu',
  params: [ { name: 'num', type: { name: 'number', max: 10, min: -5, step: 3 } } ],
  exec: createExec('tsu')
};

var tsf = {
  name: 'tsf',
  params: [ { name: 'num', type: { name: 'number', allowFloat: true, max: 11.5, min: -6.5, step: 1.5 } } ],
  exec: createExec('tsf')
};

var tsn = {
  name: 'tsn'
};

var tsnDif = {
  name: 'tsn dif',
  description: 'tsn dif',
  params: [ { name: 'text', type: 'string', description: 'tsn dif text' } ],
  exec: createExec('tsnDif')
};

var tsnExt = {
  name: 'tsn ext',
  params: [ { name: 'text', type: 'string' } ],
  exec: createExec('tsnExt')
};

var tsnExte = {
  name: 'tsn exte',
  params: [ { name: 'text', type: 'string' } ],
  exec: createExec('tsnExte')
};

var tsnExten = {
  name: 'tsn exten',
  params: [ { name: 'text', type: 'string' } ],
  exec: createExec('tsnExten')
};

var tsnExtend = {
  name: 'tsn extend',
  params: [ { name: 'text', type: 'string' } ],
  exec: createExec('tsnExtend')
};

var tsnDeep = {
  name: 'tsn deep'
};

var tsnDeepDown = {
  name: 'tsn deep down'
};

var tsnDeepDownNested = {
  name: 'tsn deep down nested'
};

var tsnDeepDownNestedCmd = {
  name: 'tsn deep down nested cmd',
  exec: createExec('tsnDeepDownNestedCmd')
};

var tshidden = {
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

var tselarr = {
  name: 'tselarr',
  params: [
    { name: 'num', type: { name: 'selection', data: [ '1', '2', '3' ] } },
    { name: 'arr', type: { name: 'array', subtype: 'string' } }
  ],
  exec: createExec('tselarr')
};

var tsm = {
  name: 'tsm',
  description: 'a 3-param test selection|string|number',
  params: [
    { name: 'abc', type: { name: 'selection', data: [ 'a', 'b', 'c' ] } },
    { name: 'txt', type: 'string' },
    { name: 'num', type: { name: 'number', max: 42, min: 0 } }
  ],
  exec: createExec('tsm')
};

var tsg = {
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
      name: 'txt2',
      type: 'string',
      defaultValue: 'd',
      description: 'txt2 param',
      option: 'Second'
    },
    {
      name: 'num',
      type: { name: 'number', min: 40 },
      defaultValue: 42,
      description: 'num param',
      option: 'Second'
    }
  ],
  exec: createExec('tsg')
};

var tscook = {
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

var tslong = {
  name: 'tslong',
  description: 'long param tests to catch problems with the jsb command',
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

var tsdate = {
  name: 'tsdate',
  description: 'long param tests to catch problems with the jsb command',
  params: [
    {
      name: 'd1',
      type: 'date',
    },
    {
      name: 'd2',
      type: {
        name: 'date',
        min: '1 jan 2000',
        max: '28 feb 2000',
        step: 2
      }
    },
  ],
  exec: createExec('tsdate')
};

var tsfail = {
  name: 'tsfail',
  description: 'test errors',
  params: [
    {
      name: 'method',
      type: {
        name: 'selection',
        data: [
          'reject', 'rejecttyped',
          'throwerror', 'throwstring', 'throwinpromise',
          'noerror'
        ]
      }
    }
  ],
  exec: function(args, context) {
    if (args.method === 'reject') {
      var deferred = context.defer();
      setTimeout(function() {
        deferred.reject('rejected promise');
      }, 10);
      return deferred.promise;
    }

    if (args.method === 'rejecttyped') {
      var deferred = context.defer();
      setTimeout(function() {
        deferred.reject(context.typedData('number', 54));
      }, 10);
      return deferred.promise;
    }

    if (args.method === 'throwinpromise') {
      var deferred = context.defer();
      setTimeout(function() {
        deferred.resolve('should be lost');
      }, 10);
      return deferred.promise.then(function() {
        var t = null;
        return t.foo;
      });
    }

    if (args.method === 'throwerror') {
      throw new Error('thrown error');
    }

    if (args.method === 'throwstring') {
      throw 'thrown string';
    }

    return 'no error';
  }
};

var tsfile = {
  item: 'command',
  name: 'tsfile',
  description: 'test file params',
};

var tsfileOpen = {
  item: 'command',
  name: 'tsfile open',
  description: 'a file param in open mode',
  params: [
    {
      name: 'p1',
      type: {
        name: 'file',
        filetype: 'file',
        existing: 'yes'
      }
    }
  ],
  exec: createExec('tsfile open')
};

var tsfileSaveas = {
  item: 'command',
  name: 'tsfile saveas',
  description: 'a file param in saveas mode',
  params: [
    {
      name: 'p1',
      type: {
        name: 'file',
        filetype: 'file',
        existing: 'no'
      }
    }
  ],
  exec: createExec('tsfile saveas')
};

var tsfileSave = {
  item: 'command',
  name: 'tsfile save',
  description: 'a file param in save mode',
  params: [
    {
      name: 'p1',
      type: {
        name: 'file',
        filetype: 'file',
        existing: 'maybe'
      }
    }
  ],
  exec: createExec('tsfile save')
};

var tsfileCd = {
  item: 'command',
  name: 'tsfile cd',
  description: 'a file param in cd mode',
  params: [
    {
      name: 'p1',
      type: {
        name: 'file',
        filetype: 'directory',
        existing: 'yes'
      }
    }
  ],
  exec: createExec('tsfile cd')
};

var tsfileMkdir = {
  item: 'command',
  name: 'tsfile mkdir',
  description: 'a file param in mkdir mode',
  params: [
    {
      name: 'p1',
      type: {
        name: 'file',
        filetype: 'directory',
        existing: 'no'
      }
    }
  ],
  exec: createExec('tsfile mkdir')
};

var tsfileRm = {
  item: 'command',
  name: 'tsfile rm',
  description: 'a file param in rm mode',
  params: [
    {
      name: 'p1',
      type: {
        name: 'file',
        filetype: 'any',
        existing: 'yes'
      }
    }
  ],
  exec: createExec('tsfile rm')
};



mockCommands.commands = {};

/**
 * Registration and de-registration.
 */
mockCommands.setup = function(opts) {
  // setup/shutdown needs to register/unregister types, however that means we
  // need to re-initialize mockCommands.option1 and mockCommands.option2 with
  // the actual types
  mockCommands.option1.type = types.createType('string');
  mockCommands.option2.type = types.createType('number');
  mockCommands.option3.type = types.createType({
    name: 'selection',
    lookup: [
      { name: 'one', value: 1 },
      { name: 'two', value: 2 },
      { name: 'three', value: 3 }
    ]
  });

  types.addType(mockCommands.optionType);
  types.addType(mockCommands.optionValue);

  mockCommands.commands.tsv = canon.addCommand(tsv);
  mockCommands.commands.tsr = canon.addCommand(tsr);
  mockCommands.commands.tsrsrsr = canon.addCommand(tsrsrsr);
  mockCommands.commands.tso = canon.addCommand(tso);
  mockCommands.commands.tse = canon.addCommand(tse);
  mockCommands.commands.tsj = canon.addCommand(tsj);
  mockCommands.commands.tsb = canon.addCommand(tsb);
  mockCommands.commands.tss = canon.addCommand(tss);
  mockCommands.commands.tsu = canon.addCommand(tsu);
  mockCommands.commands.tsf = canon.addCommand(tsf);
  mockCommands.commands.tsn = canon.addCommand(tsn);
  mockCommands.commands.tsnDif = canon.addCommand(tsnDif);
  mockCommands.commands.tsnExt = canon.addCommand(tsnExt);
  mockCommands.commands.tsnExte = canon.addCommand(tsnExte);
  mockCommands.commands.tsnExten = canon.addCommand(tsnExten);
  mockCommands.commands.tsnExtend = canon.addCommand(tsnExtend);
  mockCommands.commands.tsnDeep = canon.addCommand(tsnDeep);
  mockCommands.commands.tsnDeepDown = canon.addCommand(tsnDeepDown);
  mockCommands.commands.tsnDeepDownNested = canon.addCommand(tsnDeepDownNested);
  mockCommands.commands.tsnDeepDownNestedCmd = canon.addCommand(tsnDeepDownNestedCmd);
  mockCommands.commands.tselarr = canon.addCommand(tselarr);
  mockCommands.commands.tsm = canon.addCommand(tsm);
  mockCommands.commands.tsg = canon.addCommand(tsg);
  mockCommands.commands.tshidden = canon.addCommand(tshidden);
  mockCommands.commands.tscook = canon.addCommand(tscook);
  mockCommands.commands.tslong = canon.addCommand(tslong);
  mockCommands.commands.tsdate = canon.addCommand(tsdate);
  mockCommands.commands.tsfail = canon.addCommand(tsfail);
  mockCommands.commands.tsfile = canon.addCommand(tsfile);
  mockCommands.commands.tsfileOpen = canon.addCommand(tsfileOpen);
  mockCommands.commands.tsfileSaveas = canon.addCommand(tsfileSaveas);
  mockCommands.commands.tsfileSave = canon.addCommand(tsfileSave);
  mockCommands.commands.tsfileCd = canon.addCommand(tsfileCd);
  mockCommands.commands.tsfileMkdir = canon.addCommand(tsfileMkdir);
  mockCommands.commands.tsfileRm = canon.addCommand(tsfileRm);
};

mockCommands.shutdown = function(opts) {
  canon.removeCommand(tsv);
  canon.removeCommand(tsr);
  canon.removeCommand(tsrsrsr);
  canon.removeCommand(tso);
  canon.removeCommand(tse);
  canon.removeCommand(tsj);
  canon.removeCommand(tsb);
  canon.removeCommand(tss);
  canon.removeCommand(tsu);
  canon.removeCommand(tsf);
  canon.removeCommand(tsn);
  canon.removeCommand(tsnDif);
  canon.removeCommand(tsnExt);
  canon.removeCommand(tsnExte);
  canon.removeCommand(tsnExten);
  canon.removeCommand(tsnExtend);
  canon.removeCommand(tsnDeep);
  canon.removeCommand(tsnDeepDown);
  canon.removeCommand(tsnDeepDownNested);
  canon.removeCommand(tsnDeepDownNestedCmd);
  canon.removeCommand(tselarr);
  canon.removeCommand(tsm);
  canon.removeCommand(tsg);
  canon.removeCommand(tshidden);
  canon.removeCommand(tscook);
  canon.removeCommand(tslong);
  canon.removeCommand(tsdate);
  canon.removeCommand(tsfail);
  canon.removeCommand(tsfile);
  canon.removeCommand(tsfileOpen);
  canon.removeCommand(tsfileSaveas);
  canon.removeCommand(tsfileSave);
  canon.removeCommand(tsfileCd);
  canon.removeCommand(tsfileMkdir);
  canon.removeCommand(tsfileRm);

  types.removeType(mockCommands.optionType);
  types.removeType(mockCommands.optionValue);

  mockCommands.commands = {};
};


// });
