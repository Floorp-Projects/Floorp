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

// THIS FILE IS GENERATED FROM SOURCE IN THE GCLI PROJECT
// PLEASE TALK TO SOMEONE IN DEVELOPER TOOLS BEFORE EDITING IT

const exports = {};

function test() {
  helpers.runTestModule(exports, "browser_gcli_node.js");
}

// var assert = require('../testharness/assert');
// var helpers = require('./helpers');

exports.testNode = function(options) {
  return helpers.audit(options, [
    {
      setup:    'tse ',
      check: {
        input:  'tse ',
        hints:      '<node> [options]',
        markup: 'VVVV',
        cursor: 4,
        current: 'node',
        status: 'ERROR',
        args: {
          command: { name: 'tse' },
          node: { status: 'INCOMPLETE' },
          nodes: { status: 'VALID' },
          nodes2: { status: 'VALID' }
        }
      }
    },
    {
      setup:    'tse :',
      check: {
        input:  'tse :',
        hints:       ' [options]',
        markup: 'VVVVE',
        cursor: 5,
        current: 'node',
        status: 'ERROR',
        args: {
          command: { name: 'tse' },
          node: {
            arg: ' :',
            status: 'ERROR',
            message: 'Syntax error in CSS query'
          },
          nodes: { status: 'VALID' },
          nodes2: { status: 'VALID' }
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
        args: {
          command: { name: 'tse' },
          node: {
            value: undefined,
            arg: ' #',
            status: 'ERROR',
            message: 'Syntax error in CSS query'
          },
          nodes: { status: 'VALID' },
          nodes2: { status: 'VALID' }
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
        args: {
          command: { name: 'tse' },
          node: {
            value: undefined,
            arg: ' .',
            status: 'ERROR',
            message: 'Syntax error in CSS query'
          },
          nodes: { status: 'VALID' },
          nodes2: { status: 'VALID' }
        }
      }
    },
    {
      setup:    'tse *',
      check: {
        input:  'tse *',
        hints:       ' [options]',
        markup: 'VVVVE',
        cursor: 5,
        current: 'node',
        status: 'ERROR',
        args: {
          command: { name: 'tse' },
          node: {
            value: undefined,
            arg: ' *',
            status: 'ERROR'
            // message: 'Too many matches (128)'
          },
          nodes: { status: 'VALID' },
          nodes2: { status: 'VALID' }
        }
      }
    }
  ]);
};

exports.testNodeDom = function(options) {
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
        args: {
          command: { name: 'tse' },
          node: { arg: ' :root', status: 'VALID' },
          nodes: { status: 'VALID' },
          nodes2: { status: 'VALID' }
        }
      }
    },
    {
      setup:    'tse :root ',
      check: {
        input:  'tse :root ',
        hints:            '[options]',
        markup: 'VVVVVVVVVV',
        cursor: 10,
        current: 'node',
        status: 'VALID',
        args: {
          command: { name: 'tse' },
          node: { arg: ' :root ', status: 'VALID' },
          nodes: { status: 'VALID' },
          nodes2: { status: 'VALID' }
        }
      },
      exec: {
      },
      post: function(output) {
        if (!options.isRemote) {
          assert.is(output.args.node.tagName, 'HTML', ':root tagName');
        }
      }
    },
    {
      setup:    'tse #gcli-nomatch',
      check: {
        input:  'tse #gcli-nomatch',
        hints:                   ' [options]',
        markup: 'VVVVIIIIIIIIIIIII',
        cursor: 17,
        current: 'node',
        status: 'ERROR',
        args: {
          command: { name: 'tse' },
          node: {
            value: undefined,
            arg: ' #gcli-nomatch',
            status: 'INCOMPLETE',
            message: 'No matches'
          },
          nodes: { status: 'VALID' },
          nodes2: { status: 'VALID' }
        }
      }
    }
  ]);
};

exports.testNodes = function(options) {
  return helpers.audit(options, [
    {
      setup:    'tse :root --nodes *',
      check: {
        input:  'tse :root --nodes *',
        hints:                       ' [options]',
        markup: 'VVVVVVVVVVVVVVVVVVV',
        current: 'nodes',
        status: 'VALID',
        args: {
          command: { name: 'tse' },
          node: { arg: ' :root', status: 'VALID' },
          nodes: { arg: ' --nodes *', status: 'VALID' },
          nodes2: { status: 'VALID' }
        }
      },
      exec: {
      },
      post: function(output) {
        if (!options.isRemote) {
          assert.is(output.args.node.tagName, 'HTML', ':root tagName');
          assert.ok(output.args.nodes.length > 3, 'nodes length');
          assert.is(output.args.nodes2.length, 0, 'nodes2 length');
        }

        assert.is(output.data.args.node, ':root', 'node data');
        assert.is(output.data.args.nodes, '*', 'nodes data');
        assert.is(output.data.args.nodes2, 'Error', 'nodes2 data');
      }
    },
    {
      setup:    'tse :root --nodes2 div',
      check: {
        input:  'tse :root --nodes2 div',
        hints:                       ' [options]',
        markup: 'VVVVVVVVVVVVVVVVVVVVVV',
        cursor: 22,
        current: 'nodes2',
        status: 'VALID',
        args: {
          command: { name: 'tse' },
          node: { arg: ' :root', status: 'VALID' },
          nodes: { status: 'VALID' },
          nodes2: { arg: ' --nodes2 div', status: 'VALID' }
        }
      },
      exec: {
      },
      post: function(output) {
        if (!options.isRemote) {
          assert.is(output.args.node.tagName, 'HTML', ':root tagName');
          assert.is(output.args.nodes.length, 0, 'nodes length');
          assert.is(output.args.nodes2.item(0).tagName, 'DIV', 'div tagName');
        }

        assert.is(output.data.args.node, ':root', 'node data');
        assert.is(output.data.args.nodes, 'Error', 'nodes data');
        assert.is(output.data.args.nodes2, 'div', 'nodes2 data');
      }
    },
    {
      setup:    'tse --nodes ffff',
      check: {
        input:  'tse --nodes ffff',
        hints:                  ' <node> [options]',
        markup: 'VVVVIIIIIIIVIIII',
        cursor: 16,
        current: 'nodes',
        status: 'ERROR',
        args: {
          command: { name: 'tse' },
          node: {
            value: undefined,
            arg: '',
            status: 'INCOMPLETE'
          },
          nodes: {
            value: undefined,
            arg: ' --nodes ffff',
            status: 'INCOMPLETE',
            message: 'No matches'
          },
          nodes2: { arg: '', status: 'VALID', message: '' }
        }
      }
    },
    {
      setup:    'tse --nodes2 ffff',
      check: {
        input:  'tse --nodes2 ffff',
        hints:                   ' <node> [options]',
        markup: 'VVVVVVVVVVVVVVVVV',
        cursor: 17,
        current: 'nodes2',
        status: 'ERROR',
        args: {
          command: { name: 'tse' },
          node: {
            value: undefined,
            arg: '',
            status: 'INCOMPLETE'
          },
          nodes: { arg: '', status: 'VALID', message: '' },
          nodes2: { arg: ' --nodes2 ffff', status: 'VALID', message: '' }
        }
      }
    },
  ]);
};
