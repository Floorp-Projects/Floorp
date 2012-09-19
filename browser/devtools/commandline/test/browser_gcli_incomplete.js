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

const TEST_URI = "data:text/html;charset=utf-8,<p id='gcli-input'>gcli-testIncomplete.js</p>";

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

exports.testBasic = function(options) {
  var requisition = options.display.requisition;

  helpers.setInput('tsu 2 extra');
  helpers.check({
    args: {
      num: { value: 2, type: 'Argument' }
    }
  });
  assert.is(requisition._unassigned.length, 1, 'single unassigned: tsu 2 extra');
  assert.is(requisition._unassigned[0].param.type.isIncompleteName, false,
          'unassigned.isIncompleteName: tsu 2 extra');

  helpers.setInput('tsu');
  helpers.check({
    args: {
      num: { value: undefined, type: 'BlankArgument' }
    }
  });

  helpers.setInput('tsg');
  helpers.check({
    args: {
      solo: { type: 'BlankArgument' },
      txt1: { type: 'BlankArgument' },
      bool: { type: 'BlankArgument' },
      txt2: { type: 'BlankArgument' },
      num: { type: 'BlankArgument' }
    }
  });
};

exports.testCompleted = function(options) {
  helpers.setInput('tsela');
  helpers.pressTab();
  helpers.check({
    args: {
      command: { name: 'tselarr', type: 'Argument' },
      num: { type: 'BlankArgument' },
      arr: { type: 'ArrayArgument' },
    }
  });

  helpers.setInput('tsn dif ');
  helpers.check({
    input:  'tsn dif ',
    hints:          '<text>',
    markup: 'VVVVVVVV',
    cursor: 8,
    status: 'ERROR',
    args: {
      command: { name: 'tsn dif', type: 'MergedArgument' },
      text: { type: 'BlankArgument', status: 'INCOMPLETE' }
    }
  });

  helpers.setInput('tsn di');
  helpers.pressTab();
  helpers.check({
    input:  'tsn dif ',
    hints:          '<text>',
    markup: 'VVVVVVVV',
    cursor: 8,
    status: 'ERROR',
    args: {
      command: { name: 'tsn dif', type: 'Argument' },
      text: { type: 'BlankArgument', status: 'INCOMPLETE' }
    }
  });

  // The above 2 tests take different routes to 'tsn dif '. The results should
  // be similar. The difference is in args.command.type.

  helpers.setInput('tsg -');
  helpers.check({
    input:  'tsg -',
    hints:       '-txt1 <solo> [options]',
    markup: 'VVVVI',
    cursor: 5,
    status: 'ERROR',
    args: {
      solo: { value: undefined, status: 'INCOMPLETE' },
      txt1: { value: undefined, status: 'VALID' },
      bool: { value: false, status: 'VALID' },
      txt2: { value: undefined, status: 'VALID' },
      num: { value: undefined, status: 'VALID' }
    }
  });

  helpers.pressTab();
  helpers.check({
    input:  'tsg --txt1 ',
    hints:             '<string> <solo> [options]',
    markup: 'VVVVIIIIIIV',
    cursor: 11,
    status: 'ERROR',
    args: {
      solo: { value: undefined, status: 'INCOMPLETE' },
      txt1: { value: undefined, status: 'INCOMPLETE' },
      bool: { value: false, status: 'VALID' },
      txt2: { value: undefined, status: 'VALID' },
      num: { value: undefined, status: 'VALID' }
    }
  });

  helpers.setInput('tsg --txt1 fred');
  helpers.check({
    input:  'tsg --txt1 fred',
    hints:                 ' <solo> [options]',
    markup: 'VVVVVVVVVVVVVVV',
    status: 'ERROR',
    args: {
      solo: { value: undefined, status: 'INCOMPLETE' },
      txt1: { value: 'fred', status: 'VALID' },
      bool: { value: false, status: 'VALID' },
      txt2: { value: undefined, status: 'VALID' },
      num: { value: undefined, status: 'VALID' }
    }
  });

  helpers.setInput('tscook key value --path path --');
  helpers.check({
    input:  'tscook key value --path path --',
    hints:                                 'domain [options]',
    markup: 'VVVVVVVVVVVVVVVVVVVVVVVVVVVVVII',
    status: 'ERROR',
    args: {
      key: { value: 'key', status: 'VALID' },
      value: { value: 'value', status: 'VALID' },
      path: { value: 'path', status: 'VALID' },
      domain: { value: undefined, status: 'VALID' },
      secure: { value: false, status: 'VALID' }
    }
  });

  helpers.setInput('tscook key value --path path --domain domain --');
  helpers.check({
    input:  'tscook key value --path path --domain domain --',
    hints:                                                 'secure [options]',
    markup: 'VVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVII',
    status: 'ERROR',
    args: {
      key: { value: 'key', status: 'VALID' },
      value: { value: 'value', status: 'VALID' },
      path: { value: 'path', status: 'VALID' },
      domain: { value: 'domain', status: 'VALID' },
      secure: { value: false, status: 'VALID' }
    }
  });
};

exports.testCase = function(options) {
  helpers.setInput('tsg AA');
  helpers.check({
    input:  'tsg AA',
    hints:        ' [options] -> aaa',
    markup: 'VVVVII',
    status: 'ERROR',
    args: {
      solo: { value: undefined, text: 'AA', status: 'INCOMPLETE' },
      txt1: { value: undefined, status: 'VALID' },
      bool: { value: false, status: 'VALID' },
      txt2: { value: undefined, status: 'VALID' },
      num: { value: undefined, status: 'VALID' }
    }
  });
};

exports.testIncomplete = function(options) {
  var requisition = options.display.requisition;

  helpers.setInput('tsm a a -');
  helpers.check({
    args: {
      abc: { value: 'a', type: 'Argument' },
      txt: { value: 'a', type: 'Argument' },
      num: { value: undefined, arg: ' -', type: 'Argument', status: 'INCOMPLETE' }
    }
  });

  helpers.setInput('tsg -');
  helpers.check({
    args: {
      solo: { type: 'BlankArgument' },
      txt1: { type: 'BlankArgument' },
      bool: { type: 'BlankArgument' },
      txt2: { type: 'BlankArgument' },
      num: { type: 'BlankArgument' }
    }
  });
  assert.is(requisition._unassigned[0], requisition.getAssignmentAt(5),
          'unassigned -');
  assert.is(requisition._unassigned.length, 1, 'single unassigned - tsg -');
  assert.is(requisition._unassigned[0].param.type.isIncompleteName, true,
          'unassigned.isIncompleteName: tsg -');
};

exports.testHidden = function(options) {
  helpers.setInput('tshidde');
  helpers.check({
    input:  'tshidde',
    hints:         ' -> tse',
    status: 'ERROR'
  });

  helpers.setInput('tshidden');
  helpers.check({
    input:  'tshidden',
    hints:          ' [options]',
    markup: 'VVVVVVVV',
    status: 'VALID',
    args: {
      visible: { value: undefined, status: 'VALID' },
      invisiblestring: { value: undefined, status: 'VALID' },
      invisibleboolean: { value: false, status: 'VALID' }
    }
  });

  helpers.setInput('tshidden --vis');
  helpers.check({
    input:  'tshidden --vis',
    hints:                'ible [options]',
    markup: 'VVVVVVVVVIIIII',
    status: 'ERROR',
    args: {
      visible: { value: undefined, status: 'VALID' },
      invisiblestring: { value: undefined, status: 'VALID' },
      invisibleboolean: { value: false, status: 'VALID' }
    }
  });

  helpers.setInput('tshidden --invisiblestrin');
  helpers.check({
    input:  'tshidden --invisiblestrin',
    hints:                           ' [options]',
    markup: 'VVVVVVVVVEEEEEEEEEEEEEEEE',
    status: 'ERROR',
    args: {
      visible: { value: undefined, status: 'VALID' },
      invisiblestring: { value: undefined, status: 'VALID' },
      invisibleboolean: { value: false, status: 'VALID' }
    }
  });

  helpers.setInput('tshidden --invisiblestring');
  helpers.check({
    input:  'tshidden --invisiblestring',
    hints:                            ' <string> [options]',
    markup: 'VVVVVVVVVIIIIIIIIIIIIIIIII',
    status: 'ERROR',
    args: {
      visible: { value: undefined, status: 'VALID' },
      invisiblestring: { value: undefined, status: 'INCOMPLETE' },
      invisibleboolean: { value: false, status: 'VALID' }
    }
  });

  helpers.setInput('tshidden --invisiblestring x');
  helpers.check({
    input:  'tshidden --invisiblestring x',
    hints:                              ' [options]',
    markup: 'VVVVVVVVVVVVVVVVVVVVVVVVVVVV',
    status: 'VALID',
    args: {
      visible: { value: undefined, status: 'VALID' },
      invisiblestring: { value: 'x', status: 'VALID' },
      invisibleboolean: { value: false, status: 'VALID' }
    }
  });

  helpers.setInput('tshidden --invisibleboolea');
  helpers.check({
    input:  'tshidden --invisibleboolea',
    hints:                            ' [options]',
    markup: 'VVVVVVVVVEEEEEEEEEEEEEEEEE',
    status: 'ERROR',
    args: {
      visible: { value: undefined, status: 'VALID' },
      invisiblestring: { value: undefined, status: 'VALID' },
      invisibleboolean: { value: false, status: 'VALID' }
    }
  });

  helpers.setInput('tshidden --invisibleboolean');
  helpers.check({
    input:  'tshidden --invisibleboolean',
    hints:                             ' [options]',
    markup: 'VVVVVVVVVVVVVVVVVVVVVVVVVVV',
    status: 'VALID',
    args: {
      visible: { value: undefined, status: 'VALID' },
      invisiblestring: { value: undefined, status: 'VALID' },
      invisibleboolean: { value: true, status: 'VALID' }
    }
  });

  helpers.setInput('tshidden --visible xxx');
  helpers.check({
    input:  'tshidden --visible xxx',
    markup: 'VVVVVVVVVVVVVVVVVVVVVV',
    status: 'VALID',
    hints:  '',
    args: {
      visible: { value: 'xxx', status: 'VALID' },
      invisiblestring: { value: undefined, status: 'VALID' },
      invisibleboolean: { value: false, status: 'VALID' }
    }
  });
};

// });
