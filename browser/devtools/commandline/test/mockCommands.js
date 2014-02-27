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

// <INJECTED SOURCE:END>


var mockCommands = {};

// We use an alias for exports here because this module is used in Firefox
// mochitests where we don't have define/require

/**
 * Registration and de-registration.
 */
mockCommands.setup = function(requisition) {
  mockCommands.items.forEach(function(item) {
    if (item.item === 'command') {
      requisition.canon.addCommand(item);
    }
    else if (item.item === 'type') {
      requisition.types.addType(item);
    }
    else {
      console.error('Ignoring item ', item);
    }
  });
};

mockCommands.shutdown = function(requisition) {
  mockCommands.items.forEach(function(item) {
    if (item.item === 'command') {
      requisition.canon.removeCommand(item);
    }
    else if (item.item === 'type') {
      requisition.types.removeType(item);
    }
    else {
      console.error('Ignoring item ', item);
    }
  });
};

function createExec(name) {
  return function(args, executionContext) {
    var argsOut = Object.keys(args).map(function(key) {
      return key + '=' + args[key];
    }).join(', ');
    return 'Exec: ' + name + ' ' + argsOut;
  };
}

mockCommands.items = [
  {
    item: 'type',
    name: 'optionType',
    parent: 'selection',
    lookup: [
      {
        name: 'option1',
        value: 'string'
      },
      {
        name: 'option2',
        value: 'number'
      },
      {
        name: 'option3',
        value: {
          name: 'selection',
          lookup: [
            { name: 'one', value: 1 },
            { name: 'two', value: 2 },
            { name: 'three', value: 3 }
          ]
        }
      }
    ]
  },
  {
    item: 'type',
    name: 'optionValue',
    parent: 'delegate',
    delegateType: function(executionContext) {
      if (executionContext != null) {
        var option = executionContext.getArgsObject().optionType;
        if (option != null) {
          return option;
        }
      }
      return 'blank';
    }
  },
  {
    item: 'command',
    name: 'tsv',
    params: [
      { name: 'optionType', type: 'optionType' },
      { name: 'optionValue', type: 'optionValue' }
    ],
    exec: createExec('tsv')
  },
  {
    item: 'command',
    name: 'tsr',
    params: [ { name: 'text', type: 'string' } ],
    exec: createExec('tsr')
  },
  {
    item: 'command',
    name: 'tsrsrsr',
    params: [
      { name: 'p1', type: 'string' },
      { name: 'p2', type: 'string' },
      { name: 'p3', type: { name: 'string', allowBlank: true} },
    ],
    exec: createExec('tsrsrsr')
  },
  {
    item: 'command',
    name: 'tso',
    params: [ { name: 'text', type: 'string', defaultValue: null } ],
    exec: createExec('tso')
  },
  {
    item: 'command',
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
  },
  {
    item: 'command',
    name: 'tsj',
    params: [ { name: 'javascript', type: 'javascript' } ],
    exec: createExec('tsj')
  },
  {
    item: 'command',
    name: 'tsb',
    params: [ { name: 'toggle', type: 'boolean' } ],
    exec: createExec('tsb')
  },
  {
    item: 'command',
    name: 'tss',
    exec: createExec('tss')
  },
  {
    item: 'command',
    name: 'tsu',
    params: [
      {
        name: 'num',
        type: {
          name: 'number',
          max: 10,
          min: -5,
          step: 3
        }
      }
    ],
    exec: createExec('tsu')
  },
  {
    item: 'command',
    name: 'tsf',
    params: [
      {
        name: 'num',
        type: {
          name: 'number',
          allowFloat: true,
          max: 11.5,
          min: -6.5,
          step: 1.5
        }
      }
    ],
    exec: createExec('tsf')
  },
  {
    item: 'command',
    name: 'tsn'
  },
  {
    item: 'command',
    name: 'tsn dif',
    params: [ { name: 'text', type: 'string', description: 'tsn dif text' } ],
    exec: createExec('tsnDif')
  },
  {
    item: 'command',
    name: 'tsn hidden',
    hidden: true,
    exec: createExec('tsnHidden')
  },
  {
    item: 'command',
    name: 'tsn ext',
    params: [ { name: 'text', type: 'string' } ],
    exec: createExec('tsnExt')
  },
  {
    item: 'command',
    name: 'tsn exte',
    params: [ { name: 'text', type: 'string' } ],
    exec: createExec('tsnExte')
  },
  {
    item: 'command',
    name: 'tsn exten',
    params: [ { name: 'text', type: 'string' } ],
    exec: createExec('tsnExten')
  },
  {
    item: 'command',
    name: 'tsn extend',
    params: [ { name: 'text', type: 'string' } ],
    exec: createExec('tsnExtend')
  },
  {
    item: 'command',
    name: 'tsn deep'
  },
  {
    item: 'command',
    name: 'tsn deep down'
  },
  {
    item: 'command',
    name: 'tsn deep down nested'
  },
  {
    item: 'command',
    name: 'tsn deep down nested cmd',
    exec: createExec('tsnDeepDownNestedCmd')
  },
  {
    item: 'command',
    name: 'tshidden',
    hidden: true,
    params: [
      {
        group: 'Options',
        params: [
          {
            name: 'visible',
            type: 'string',
            short: 'v',
            defaultValue: null,
            description: 'visible'
          },
          {
            name: 'invisiblestring',
            type: 'string',
            short: 'i',
            description: 'invisiblestring',
            defaultValue: null,
            hidden: true
          },
          {
            name: 'invisibleboolean',
            short: 'b',
            type: 'boolean',
            description: 'invisibleboolean',
            hidden: true
          }
        ]
      }
    ],
    exec: createExec('tshidden')
  },
  {
    item: 'command',
    name: 'tselarr',
    params: [
      { name: 'num', type: { name: 'selection', data: [ '1', '2', '3' ] } },
      { name: 'arr', type: { name: 'array', subtype: 'string' } }
    ],
    exec: createExec('tselarr')
  },
  {
    item: 'command',
    name: 'tsm',
    description: 'a 3-param test selection|string|number',
    params: [
      { name: 'abc', type: { name: 'selection', data: [ 'a', 'b', 'c' ] } },
      { name: 'txt', type: 'string' },
      { name: 'num', type: { name: 'number', max: 42, min: 0 } }
    ],
    exec: createExec('tsm')
  },
  {
    item: 'command',
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
  },
  {
    item: 'command',
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
  },
  {
    item: 'command',
    name: 'tslong',
    description: 'long param tests to catch problems with the jsb command',
    params: [
      {
        name: 'msg',
        type: 'string',
        description: 'msg Desc'
      },
      {
        group: 'Options Desc',
        params: [
          {
            name: 'num',
            short: 'n',
            type: 'number',
            description: 'num Desc',
            defaultValue: 2
          },
          {
            name: 'sel',
            short: 's',
            type: {
              name: 'selection',
              lookup: [
                { name: 'space', value: ' ' },
                { name: 'tab', value: '\t' }
              ]
            },
            description: 'sel Desc',
            defaultValue: ' '
          },
          {
            name: 'bool',
            short: 'b',
            type: 'boolean',
            description: 'bool Desc'
          },
          {
            name: 'num2',
            short: 'm',
            type: 'number',
            description: 'num2 Desc',
            defaultValue: -1
          },
          {
            name: 'bool2',
            short: 'c',
            type: 'boolean',
            description: 'bool2 Desc'
          },
          {
            name: 'sel2',
            short: 't',
            type: {
              name: 'selection',
              data: [ 'collapse', 'basic', 'with space', 'with two spaces' ]
            },
            description: 'sel2 Desc',
            defaultValue: 'collapse'
          }
        ]
      }
    ],
    exec: createExec('tslong')
  },
  {
    item: 'command',
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
  },
  {
    item: 'command',
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
      var deferred;
      if (args.method === 'reject') {
        deferred = context.defer();
        setTimeout(function() {
          deferred.reject('rejected promise');
        }, 10);
        return deferred.promise;
      }

      if (args.method === 'rejecttyped') {
        deferred = context.defer();
        setTimeout(function() {
          deferred.reject(context.typedData('number', 54));
        }, 10);
        return deferred.promise;
      }

      if (args.method === 'throwinpromise') {
        deferred = context.defer();
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
  },
  {
    item: 'command',
    name: 'tsfile',
    description: 'test file params',
  },
  {
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
  },
  {
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
  },
  {
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
  },
  {
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
  },
  {
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
  },
  {
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
  },
  {
    item: 'command',
    name: 'tsslow',
    params: [
      {
        name: 'hello',
        type: {
          name: 'selection',
          data: function(context) {
            var deferred = context.defer();

            var resolve = function() {
              deferred.resolve([
                'Shalom', 'Namasté', 'Hallo', 'Dydd-da',
                'Chào', 'Hej', 'Saluton', 'Sawubona'
              ]);
            };

            setTimeout(resolve, 10);
            return deferred.promise;
          }
        }
      }
    ],
    exec: function(args, context) {
      return 'Test completed';
    }
  }
];
