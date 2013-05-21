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

const TEST_URI = "data:text/html;charset=utf-8,<p id='gcli-input'>gcli-testDate.js</p>";

function test() {
  helpers.addTabWithToolbar(TEST_URI, function(options) {
    return helpers.runTests(options, exports);
  }).then(finish);
}

// <INJECTED SOURCE:END>

'use strict';

// var assert = require('test/assert');

var types = require('gcli/types');
var Argument = require('gcli/argument').Argument;
var Status = require('gcli/types').Status;

// var helpers = require('gclitest/helpers');
// var mockCommands = require('gclitest/mockCommands');

exports.setup = function(options) {
  mockCommands.setup();
};

exports.shutdown = function(options) {
  mockCommands.shutdown();
};


exports.testParse = function(options) {
  var date = types.createType('date');
  return date.parse(new Argument('now')).then(function(conversion) {
    // Date comparison - these 2 dates may not be the same, but how close is
    // close enough? If this test takes more than 30secs to run the it will
    // probably time out, so we'll assume that these 2 values must be within
    // 1 min of each other
    var gap = new Date().getTime() - conversion.value.getTime();
    assert.ok(gap < 60000, 'now is less than a minute away');

    assert.is(conversion.getStatus(), Status.VALID, 'now parse');
  });
};

exports.testMaxMin = function(options) {
  var max = new Date();
  var min = new Date();
  var date = types.createType({ name: 'date', max: max, min: min });
  assert.is(date.getMax(), max, 'max setup');

  var incremented = date.increment(min);
  assert.is(incremented, max, 'incremented');
};

exports.testIncrement = function(options) {
  var date = types.createType('date');
  return date.parse(new Argument('now')).then(function(conversion) {
    var plusOne = date.increment(conversion.value);
    var minusOne = date.decrement(plusOne);

    // See comments in testParse
    var gap = new Date().getTime() - minusOne.getTime();
    assert.ok(gap < 60000, 'now is less than a minute away');
  });
};

exports.testInput = function(options) {
  helpers.audit(options, [
    {
      setup:    'tsdate 2001-01-01 1980-01-03',
      check: {
        input:  'tsdate 2001-01-01 1980-01-03',
        hints:                              '',
        markup: 'VVVVVVVVVVVVVVVVVVVVVVVVVVVV',
        status: 'VALID',
        message: '',
        args: {
          command: { name: 'tsdate' },
          d1: {
            value: function(d1) {
              assert.is(d1.getFullYear(), 2001, 'd1 year');
              assert.is(d1.getMonth(), 0, 'd1 month');
              assert.is(d1.getDate(), 1, 'd1 date');
              assert.is(d1.getHours(), 0, 'd1 hours');
              assert.is(d1.getMinutes(), 0, 'd1 minutes');
              assert.is(d1.getSeconds(), 0, 'd1 seconds');
              assert.is(d1.getMilliseconds(), 0, 'd1 millis');
            },
            arg: ' 2001-01-01',
            status: 'VALID',
            message: ''
          },
          d2: {
            value: function(d2) {
              assert.is(d2.getFullYear(), 1980, 'd1 year');
              assert.is(d2.getMonth(), 0, 'd1 month');
              assert.is(d2.getDate(), 3, 'd1 date');
              assert.is(d2.getHours(), 0, 'd1 hours');
              assert.is(d2.getMinutes(), 0, 'd1 minutes');
              assert.is(d2.getSeconds(), 0, 'd1 seconds');
              assert.is(d2.getMilliseconds(), 0, 'd1 millis');
            },
            arg: ' 1980-01-03',
            status: 'VALID',
            message: ''
          },
        }
      },
      exec: {
        output: [ /^Exec: tsdate/, /2001/, /1980/ ],
        completed: true,
        type: 'string',
        error: false
      }
    }
  ]);
};

exports.testIncrDecr = function(options) {
  helpers.audit(options, [
    {
      setup:    'tsdate 2001-01-01<UP>',
      check: {
        input:  'tsdate 2001-01-02',
        hints:                    ' <d2>',
        markup: 'VVVVVVVVVVVVVVVVV',
        status: 'ERROR',
        message: '',
        args: {
          command: { name: 'tsdate' },
          d1: {
            value: function(d1) {
              assert.is(d1.getFullYear(), 2001, 'd1 year');
              assert.is(d1.getMonth(), 0, 'd1 month');
              assert.is(d1.getDate(), 2, 'd1 date');
              assert.is(d1.getHours(), 0, 'd1 hours');
              assert.is(d1.getMinutes(), 0, 'd1 minutes');
              assert.is(d1.getSeconds(), 0, 'd1 seconds');
              assert.is(d1.getMilliseconds(), 0, 'd1 millis');
            },
            arg: ' 2001-01-02',
            status: 'VALID',
            message: ''
          },
          d2: {
            value: undefined,
            status: 'INCOMPLETE',
            message: ''
          },
        }
      }
    },
    {
      // Check wrapping on decrement
      setup:    'tsdate 2001-02-01<DOWN>',
      check: {
        input:  'tsdate 2001-01-31',
        hints:                    ' <d2>',
        markup: 'VVVVVVVVVVVVVVVVV',
        status: 'ERROR',
        message: '',
        args: {
          command: { name: 'tsdate' },
          d1: {
            value: function(d1) {
              assert.is(d1.getFullYear(), 2001, 'd1 year');
              assert.is(d1.getMonth(), 0, 'd1 month');
              assert.is(d1.getDate(), 31, 'd1 date');
              assert.is(d1.getHours(), 0, 'd1 hours');
              assert.is(d1.getMinutes(), 0, 'd1 minutes');
              assert.is(d1.getSeconds(), 0, 'd1 seconds');
              assert.is(d1.getMilliseconds(), 0, 'd1 millis');
            },
            arg: ' 2001-01-31',
            status: 'VALID',
            message: ''
          },
          d2: {
            value: undefined,
            status: 'INCOMPLETE',
            message: ''
          },
        }
      }
    },
    {
      // Check 'max' value capping on increment
      setup:    'tsdate 2001-02-01 "27 feb 2000"<UP>',
      check: {
        input:  'tsdate 2001-02-01 "2000-02-28"',
        hints:                                '',
        markup: 'VVVVVVVVVVVVVVVVVVVVVVVVVVVVVV',
        status: 'VALID',
        message: '',
        args: {
          command: { name: 'tsdate' },
          d1: {
            value: function(d1) {
              assert.is(d1.getFullYear(), 2001, 'd1 year');
              assert.is(d1.getMonth(), 1, 'd1 month');
              assert.is(d1.getDate(), 1, 'd1 date');
              assert.is(d1.getHours(), 0, 'd1 hours');
              assert.is(d1.getMinutes(), 0, 'd1 minutes');
              assert.is(d1.getSeconds(), 0, 'd1 seconds');
              assert.is(d1.getMilliseconds(), 0, 'd1 millis');
            },
            arg: ' 2001-02-01',
            status: 'VALID',
            message: ''
          },
          d2: {
            value: function(d1) {
              assert.is(d1.getFullYear(), 2000, 'd1 year');
              assert.is(d1.getMonth(), 1, 'd1 month');
              assert.is(d1.getDate(), 28, 'd1 date');
              assert.is(d1.getHours(), 0, 'd1 hours');
              assert.is(d1.getMinutes(), 0, 'd1 minutes');
              assert.is(d1.getSeconds(), 0, 'd1 seconds');
              assert.is(d1.getMilliseconds(), 0, 'd1 millis');
            },
            arg: ' "2000-02-28"',
            status: 'VALID',
            message: ''
          },
        }
      }
    }
  ]);
};


// });
