/*
 * Copyright 2009-2011 Mozilla Foundation and contributors
 * Licensed under the New BSD license. See LICENSE.txt or:
 * http://opensource.org/licenses/BSD-3-Clause
 */

// define(function(require, exports, module) {

// <INJECTED SOURCE:START>

// THIS FILE IS GENERATED FROM SOURCE IN THE GCLI PROJECT
// DO NOT EDIT IT DIRECTLY

var exports = {};

const TEST_URI = "data:text/html;charset=utf-8,<p id='gcli-input'>gcli-testNode.js</p>";

function test() {
  var tests = Object.keys(exports);
  // Push setup to the top and shutdown to the bottom
  tests.sort(function(t1, t2) {
    if (t1 == "setup" || t2 == "shutdown") return -1;
    if (t2 == "setup" || t1 == "shutdown") return 1;
    return 0;
  });
  info("Running tests: " + tests.join(", "))
  tests = tests.map(function(test) { return exports[test]; });
  DeveloperToolbarTest.test(TEST_URI, tests, true);
}

// <INJECTED SOURCE:END>


// var assert = require('test/assert');
// var helpers = require('gclitest/helpers');
// var mockCommands = require('gclitest/mockCommands');


exports.setup = function(options) {
  mockCommands.setup();
  helpers.setup(options);
};

exports.shutdown = function(options) {
  mockCommands.shutdown();
  helpers.shutdown(options);
};

exports.testNode = function(options) {
  var requisition = options.display.requisition;

  helpers.setInput('tse ');
  helpers.check({
    input:  'tse ',
    hints:      '<node> [options]',
    markup: 'VVVV',
    cursor: 4,
    current: 'node',
    status: 'ERROR',
    args: {
      command: { name: 'tse' },
      node: { status: 'INCOMPLETE', message: '' },
      nodes: { status: 'VALID' },
      nodes2: { status: 'VALID' }
    }
  });

  helpers.setInput('tse :');
  helpers.check({
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
  });

  if (options.isJsdom) {
    assert.log('skipping node tests because jsdom');
  }
  else {
    helpers.setInput('tse :root');
    helpers.check({
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
    });

    helpers.setInput('tse :root ');
    helpers.check({
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
    });
    assert.is(requisition.getAssignment('node').value.tagName,
            'HTML',
            'root id');

    helpers.setInput('tse #gcli-nomatch');
    helpers.check({
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
    });
  }

  helpers.setInput('tse #');
  helpers.check({
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
  });

  helpers.setInput('tse .');
  helpers.check({
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
  });

  helpers.setInput('tse *');
  helpers.check({
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
        status: 'ERROR',
        // message: 'Too many matches (128)'
      },
      nodes: { status: 'VALID' },
      nodes2: { status: 'VALID' }
    }
  });
};

exports.testNodes = function(options) {
  var requisition = options.display.requisition;

  if (options.isJsdom) {
    assert.log('skipping node tests because jsdom');
    return;
  }

  helpers.setInput('tse :root --nodes *');
  helpers.check({
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
  });
  assert.is(requisition.getAssignment('node').value.tagName,
          'HTML',
          '#gcli-input id');

  helpers.setInput('tse :root --nodes2 div');
  helpers.check({
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
  });
  assert.is(requisition.getAssignment('node').value.tagName,
          'HTML',
          'root id');

  helpers.setInput('tse --nodes ffff');
  helpers.check({
    input:  'tse --nodes ffff',
    hints:                  ' <node> [options]',
    markup: 'VVVVIIIIIIIVIIII',
    cursor: 16,
    current: 'nodes',
    status: 'ERROR',
    outputState: 'false:default',
    tooltipState: 'true:isError',
    args: {
      command: { name: 'tse' },
      node: { value: undefined, arg: '', status: 'INCOMPLETE', message: '' },
      nodes: { value: undefined, arg: ' --nodes ffff', status: 'INCOMPLETE', message: 'No matches' },
      nodes2: { arg: '', status: 'VALID', message: '' },
    }
  });
  /*
  test.is(requisition.getAssignment('nodes2').value.constructor.name,
          'NodeList',
          '#gcli-input id');
  */

  helpers.setInput('tse --nodes2 ffff');
  helpers.check({
    input:  'tse --nodes2 ffff',
    hints:                   ' <node> [options]',
    markup: 'VVVVVVVVVVVVVVVVV',
    cursor: 17,
    current: 'nodes2',
    status: 'ERROR',
    outputState: 'false:default',
    tooltipState: 'false:default',
    args: {
      command: { name: 'tse' },
      node: { value: undefined, arg: '', status: 'INCOMPLETE', message: '' },
      nodes: { arg: '', status: 'VALID', message: '' },
      nodes2: { arg: ' --nodes2 ffff', status: 'VALID', message: '' },
    }
  });
  /*
  test.is(requisition.getAssignment('nodes').value.constructor.name,
          'NodeList',
          '#gcli-input id');
  test.is(requisition.getAssignment('nodes2').value.constructor.name,
          'NodeList',
          '#gcli-input id');
  */
};


// });
