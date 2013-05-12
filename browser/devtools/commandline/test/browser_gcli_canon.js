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

const TEST_URI = "data:text/html;charset=utf-8,<p id='gcli-input'>gcli-testCanon.js</p>";

function test() {
  helpers.addTabWithToolbar(TEST_URI, function(options) {
    return helpers.runTests(options, exports);
  }).then(finish);
}

// <INJECTED SOURCE:END>

'use strict';

// var helpers = require('gclitest/helpers');
var canon = require('gcli/canon');
// var assert = require('test/assert');
var Canon = canon.Canon;

var startCount = undefined;
var events = undefined;

var canonChange = function(ev) {
  events++;
};

exports.setup = function(options) {
  startCount = canon.getCommands().length;
  events = 0;
};

exports.shutdown = function(options) {
  startCount = undefined;
  events = undefined;
};

exports.testAddRemove1 = function(options) {
  return helpers.audit(options, [
    {
      name: 'testadd add',
      setup: function() {
        canon.onCanonChange.add(canonChange);

        canon.addCommand({
          name: 'testadd',
          exec: function() {
            return 1;
          }
        });

        assert.is(canon.getCommands().length,
                  startCount + 1,
                  'add command success');
        assert.is(events, 1, 'add event');

        return helpers.setInput(options, 'testadd');
      },
      check: {
        input:  'testadd',
        hints:         '',
        markup: 'VVVVVVV',
        cursor: 7,
        current: '__command',
        status: 'VALID',
        predictions: [ ],
        unassigned: [ ],
        args: { }
      },
      exec: {
        output: /^1$/
      }
    },
    {
      name: 'testadd alter',
      setup: function() {
        canon.addCommand({
          name: 'testadd',
          exec: function() {
            return 2;
          }
        });

        assert.is(canon.getCommands().length,
                  startCount + 1,
                  'read command success');
        assert.is(events, 2, 'read event');

        return helpers.setInput(options, 'testadd');
      },
      check: {
        input:  'testadd',
        hints:         '',
        markup: 'VVVVVVV',
      },
      exec: {
        output: '2'
      }
    },
    {
      name: 'testadd remove',
      setup: function() {
        canon.removeCommand('testadd');

        assert.is(canon.getCommands().length,
                  startCount,
                  'remove command success');
        assert.is(events, 3, 'remove event');

        return helpers.setInput(options, 'testadd');
      },
      check: {
        typed: 'testadd',
        cursor: 7,
        current: '__command',
        status: 'ERROR',
        unassigned: [ ],
      }
    }
  ]);
};

exports.testAddRemove2 = function(options) {
  canon.addCommand({
    name: 'testadd',
    exec: function() {
      return 3;
    }
  });

  assert.is(canon.getCommands().length,
            startCount + 1,
            'rereadd command success');
  assert.is(events, 4, 'rereadd event');

  return helpers.audit(options, [
    {
      setup: 'testadd',
      exec: {
        output: /^3$/
      },
      post: function() {
        canon.removeCommand({
          name: 'testadd'
        });

        assert.is(canon.getCommands().length,
                  startCount,
                  'reremove command success');
        assert.is(events, 5, 'reremove event');
      }
    },
    {
      setup: 'testadd',
      check: {
        typed: 'testadd',
        status: 'ERROR'
      }
    }
  ]);
};

exports.testAddRemove3 = function(options) {
  canon.removeCommand({ name: 'nonexistant' });
  assert.is(canon.getCommands().length,
            startCount,
            'nonexistant1 command success');
  assert.is(events, 5, 'nonexistant1 event');

  canon.removeCommand('nonexistant');
  assert.is(canon.getCommands().length,
            startCount,
            'nonexistant2 command success');
  assert.is(events, 5, 'nonexistant2 event');

  canon.onCanonChange.remove(canonChange);
};

exports.testAltCanon = function(options) {
  var altCanon = new Canon();

  var tss = {
    name: 'tss',
    params: [
      { name: 'str', type: 'string' },
      { name: 'num', type: 'number' },
      { name: 'opt', type: { name: 'selection', data: [ '1', '2', '3' ] } },
    ],
    exec: function(args, context) {
      return context.commandName + ':' +
              args.str + ':' + args.num + ':' + args.opt;
    }
  };
  altCanon.addCommand(tss);

  var commandSpecs = altCanon.getCommandSpecs();
  assert.is(JSON.stringify(commandSpecs),
            '{"tss":{"name":"tss","params":[' +
              '{"name":"str","type":"string"},' +
              '{"name":"num","type":"number"},' +
              '{"name":"opt","type":{"name":"selection","data":["1","2","3"]}}]}}',
            'JSON.stringify(commandSpecs)');

  var remoter = function(args, context) {
    assert.is(context.commandName, 'tss', 'commandName is tss');

    var cmd = altCanon.getCommand(context.commandName);
    return cmd.exec(args, context);
  };

  canon.addProxyCommands('proxy', commandSpecs, remoter, 'test');

  var parent = canon.getCommand('proxy');
  assert.is(parent.name, 'proxy', 'Parent command called proxy');

  var child = canon.getCommand('proxy tss');
  assert.is(child.name, 'proxy tss', 'child command called proxy tss');

  return helpers.audit(options, [
    {
      setup:    'proxy tss foo 6 3',
      check: {
        input:  'proxy tss foo 6 3',
        hints:                    '',
        markup: 'VVVVVVVVVVVVVVVVV',
        cursor: 17,
        status: 'VALID',
        args: {
          str: { value: 'foo', status: 'VALID' },
          num: { value: 6, status: 'VALID' },
          opt: { value: '3', status: 'VALID' }
        }
      },
      exec: {
        output: 'tss:foo:6:3'
      },
      post: function() {
        canon.removeCommand('proxy');
        canon.removeCommand('proxy tss');

        assert.is(canon.getCommand('proxy'), undefined, 'remove proxy');
        assert.is(canon.getCommand('proxy tss'), undefined, 'remove proxy tss');
      }
    }
  ]);
};


// });
