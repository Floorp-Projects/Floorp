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
  helpers.addTabWithToolbar(TEST_URI, function(options) {
    return helpers.runTests(options, exports);
  }).then(finish);
}

// <INJECTED SOURCE:END>

'use strict';

// var helpers = require('gclitest/helpers');
// var mockCommands = require('gclitest/mockCommands');

exports.setup = function(options) {
  mockCommands.setup();
};

exports.shutdown = function(options) {
  mockCommands.shutdown();
};

exports.testOptions = function(options) {
  return helpers.audit(options, [
    {
      setup:    'tslong',
      check: {
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
      }
    }
  ]);
};


// });

