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

var exports = {};

var TEST_URI = "data:text/html;charset=utf-8,<p id='gcli-input'>gcli-testDate.js</p>";

function test() {
  return Task.spawn(function() {
    let options = yield helpers.openTab(TEST_URI);
    yield helpers.openToolbar(options);
    gcli.addItems(mockCommands.items);

    yield helpers.runTests(options, exports);

    gcli.removeItems(mockCommands.items);
    yield helpers.closeToolbar(options);
    yield helpers.closeTab(options);
  }).then(finish, helpers.handleError);
}

// <INJECTED SOURCE:END>

// var assert = require('../testharness/assert');
// var helpers = require('./helpers');

var Status = require('gcli/types/types').Status;

exports.testParse = function(options) {
  var date = options.requisition.system.types.createType('date');
  return date.parseString('now').then(function(conversion) {
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
  var types = options.requisition.system.types;
  var date = types.createType({ name: 'date', max: max, min: min });
  assert.is(date.getMax(), max, 'max setup');

  var incremented = date.increment(min);
  assert.is(incremented, max, 'incremented');
};

exports.testIncrement = function(options) {
  var date = options.requisition.system.types.createType('date');
  return date.parseString('now').then(function(conversion) {
    var plusOne = date.increment(conversion.value);
    var minusOne = date.decrement(plusOne);

    // See comments in testParse
    var gap = new Date().getTime() - minusOne.getTime();
    assert.ok(gap < 60000, 'now is less than a minute away');
  });
};

exports.testInput = function(options) {
  return helpers.audit(options, [
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
              assert.is(d2.getFullYear(), 1980, 'd2 year');
              assert.is(d2.getMonth(), 0, 'd2 month');
              assert.is(d2.getDate(), 3, 'd2 date');
              assert.is(d2.getHours(), 0, 'd2 hours');
              assert.is(d2.getMinutes(), 0, 'd2 minutes');
              assert.is(d2.getSeconds(), 0, 'd2 seconds');
              assert.is(d2.getMilliseconds(), 0, 'd2 millis');
            },
            arg: ' 1980-01-03',
            status: 'VALID',
            message: ''
          },
        }
      },
      exec: {
        output: [ /^Exec: tsdate/, /2001/, /1980/ ],
        type: 'string',
        error: false
      }
    },
    {
      setup:    'tsdate 2001/01/01 1980/01/03',
      check: {
        input:  'tsdate 2001/01/01 1980/01/03',
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
            arg: ' 2001/01/01',
            status: 'VALID',
            message: ''
          },
          d2: {
            value: function(d2) {
              assert.is(d2.getFullYear(), 1980, 'd2 year');
              assert.is(d2.getMonth(), 0, 'd2 month');
              assert.is(d2.getDate(), 3, 'd2 date');
              assert.is(d2.getHours(), 0, 'd2 hours');
              assert.is(d2.getMinutes(), 0, 'd2 minutes');
              assert.is(d2.getSeconds(), 0, 'd2 seconds');
              assert.is(d2.getMilliseconds(), 0, 'd2 millis');
            },
            arg: ' 1980/01/03',
            status: 'VALID',
            message: ''
          },
        }
      },
      exec: {
        output: [ /^Exec: tsdate/, /2001/, /1980/ ],
        type: 'string',
        error: false
      }
    },
    {
      setup:    'tsdate now today',
      check: {
        input:  'tsdate now today',
        hints:                  '',
        markup: 'VVVVVVVVVVVVVVVV',
        status: 'VALID',
        message: '',
        args: {
          command: { name: 'tsdate' },
          d1: {
            value: function(d1) {
              // How long should we allow between d1 and now? Mochitest will
              // time out after 30 secs, so that seems like a decent upper
              // limit, although 30 ms should probably do it. I don't think
              // reducing the limit from 30 secs will find any extra bugs
              assert.ok(d1.getTime() - new Date().getTime() < 30 * 1000,
                        'd1 time');
            },
            arg: ' now',
            status: 'VALID',
            message: ''
          },
          d2: {
            value: function(d2) {
              // See comment for d1 above
              assert.ok(d2.getTime() - new Date().getTime() < 30 * 1000,
                        'd2 time');
            },
            arg: ' today',
            status: 'VALID',
            message: ''
          },
        }
      },
      exec: {
        output: [ /^Exec: tsdate/, new Date().getFullYear() ],
        type: 'string',
        error: false
      }
    },
    {
      setup:    'tsdate yesterday tomorrow',
      check: {
        input:  'tsdate yesterday tomorrow',
        hints:                           '',
        markup: 'VVVVVVVVVVVVVVVVVVVVVVVVV',
        status: 'VALID',
        message: '',
        args: {
          command: { name: 'tsdate' },
          d1: {
            value: function(d1) {
              var compare = new Date().getTime() - (24 * 60 * 60 * 1000);
              // See comment for d1 in the test for 'tsdate now today'
              assert.ok(d1.getTime() - compare < 30 * 1000,
                        'd1 time');
            },
            arg: ' yesterday',
            status: 'VALID',
            message: ''
          },
          d2: {
            value: function(d2) {
              var compare = new Date().getTime() + (24 * 60 * 60 * 1000);
              // See comment for d1 in the test for 'tsdate now today'
              assert.ok(d2.getTime() - compare < 30 * 1000,
                        'd2 time');
            },
            arg: ' tomorrow',
            status: 'VALID',
            message: ''
          },
        }
      },
      exec: {
        output: [ /^Exec: tsdate/, new Date().getFullYear() ],
        type: 'string',
        error: false
      }
    }
  ]);
};

exports.testIncrDecr = function(options) {
  return helpers.audit(options, [
    {
      // createRequisitionAutomator doesn't fake UP/DOWN well enough
      skipRemainingIf: options.isNoDom,
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
            status: 'INCOMPLETE'
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
            status: 'INCOMPLETE'
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
            value: function(d2) {
              assert.is(d2.getFullYear(), 2000, 'd2 year');
              assert.is(d2.getMonth(), 1, 'd2 month');
              assert.is(d2.getDate(), 28, 'd2 date');
              assert.is(d2.getHours(), 0, 'd2 hours');
              assert.is(d2.getMinutes(), 0, 'd2 minutes');
              assert.is(d2.getSeconds(), 0, 'd2 seconds');
              assert.is(d2.getMilliseconds(), 0, 'd2 millis');
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
