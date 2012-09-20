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

const TEST_URI = "data:text/html;charset=utf-8,<p id='gcli-input'>gcli-testMenu.js</p>";

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

exports.testOptions = function(options) {
  helpers.setInput('tslong');
  helpers.check({
    input:  'tslong',
    markup: 'VVVVVV',
    status: 'ERROR',
    hints: ' <msg> [options]',
    args: {
      msg: { value: undefined, status: 'INCOMPLETE' },
      num: { value: undefined, status: 'VALID' },
      sel: { value: undefined, status: 'VALID' },
      bool: { value: false, status: 'VALID' },
      bool2: { value: false, status: 'VALID' },
      sel2: { value: undefined, status: 'VALID' },
      num2: { value: undefined, status: 'VALID' }
    }
  });
};


// });

