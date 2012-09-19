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

const TEST_URI = "data:text/html;charset=utf-8,<p id='gcli-input'>gcli-testCli.js</p>";

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


var Requisition = require('gcli/cli').Requisition;
var Status = require('gcli/types').Status;
// var mockCommands = require('gclitest/mockCommands');

// var assert = require('test/assert');

exports.setup = function() {
  mockCommands.setup();
};

exports.shutdown = function() {
  mockCommands.shutdown();
};


var assign1;
var assign2;
var assignC;
var requ;
var debug = false;
var status;
var statuses;

function update(input) {
  if (!requ) {
    requ = new Requisition();
  }
  requ.update(input.typed);

  if (debug) {
    console.log('####### TEST: typed="' + input.typed +
        '" cur=' + input.cursor.start +
        ' cli=', requ);
  }

  status = requ.getStatus();
  assignC = requ.getAssignmentAt(input.cursor.start);
  statuses = requ.getInputStatusMarkup(input.cursor.start).map(function(s) {
    return Array(s.string.length + 1).join(s.status.toString()[0]);
  }).join('');

  if (requ.commandAssignment.value) {
    assign1 = requ.getAssignment(0);
    assign2 = requ.getAssignment(1);
  }
  else {
    assign1 = undefined;
    assign2 = undefined;
  }
}

function verifyPredictionsContains(name, predictions) {
  return predictions.every(function(prediction) {
    return name === prediction.name;
  }, this);
}


exports.testBlank = function() {
  update({ typed: '', cursor: { start: 0, end: 0 } });
  assert.is(        '', statuses);
  assert.is(Status.ERROR, status);
  assert.is(-1, assignC.paramIndex);
  assert.is(undefined, requ.commandAssignment.value);

  update({ typed: ' ', cursor: { start: 1, end: 1 } });
  assert.is(        'V', statuses);
  assert.is(Status.ERROR, status);
  assert.is(-1, assignC.paramIndex);
  assert.is(undefined, requ.commandAssignment.value);

  update({ typed: ' ', cursor: { start: 0, end: 0 } });
  assert.is(        'V', statuses);
  assert.is(Status.ERROR, status);
  assert.is(-1, assignC.paramIndex);
  assert.is(undefined, requ.commandAssignment.value);
};

exports.testIncompleteMultiMatch = function() {
  update({ typed: 't', cursor: { start: 1, end: 1 } });
  assert.is(        'I', statuses);
  assert.is(Status.ERROR, status);
  assert.is(-1, assignC.paramIndex);
  assert.ok(assignC.getPredictions().length > 0);
  verifyPredictionsContains('tsv', assignC.getPredictions());
  verifyPredictionsContains('tsr', assignC.getPredictions());
  assert.is(undefined, requ.commandAssignment.value);
};

exports.testIncompleteSingleMatch = function() {
  update({ typed: 'tselar', cursor: { start: 6, end: 6 } });
  assert.is(        'IIIIII', statuses);
  assert.is(Status.ERROR, status);
  assert.is(-1, assignC.paramIndex);
  assert.is(1, assignC.getPredictions().length);
  assert.is('tselarr', assignC.getPredictions()[0].name);
  assert.is(undefined, requ.commandAssignment.value);
};

exports.testTsv = function() {
  update({ typed: 'tsv', cursor: { start: 3, end: 3 } });
  assert.is(        'VVV', statuses);
  assert.is(Status.ERROR, status);
  assert.is(-1, assignC.paramIndex);
  assert.is('tsv', requ.commandAssignment.value.name);

  update({ typed: 'tsv ', cursor: { start: 4, end: 4 } });
  assert.is(        'VVVV', statuses);
  assert.is(Status.ERROR, status);
  assert.is(0, assignC.paramIndex);
  assert.is('tsv', requ.commandAssignment.value.name);

  update({ typed: 'tsv ', cursor: { start: 2, end: 2 } });
  assert.is(        'VVVV', statuses);
  assert.is(Status.ERROR, status);
  assert.is(-1, assignC.paramIndex);
  assert.is('tsv', requ.commandAssignment.value.name);

  update({ typed: 'tsv o', cursor: { start: 5, end: 5 } });
  assert.is(        'VVVVI', statuses);
  assert.is(Status.ERROR, status);
  assert.is(0, assignC.paramIndex);
  assert.ok(assignC.getPredictions().length >= 2);
  assert.is(mockCommands.option1, assignC.getPredictions()[0].value);
  assert.is(mockCommands.option2, assignC.getPredictions()[1].value);
  assert.is('tsv', requ.commandAssignment.value.name);
  assert.is('o', assign1.arg.text);
  assert.is(undefined, assign1.value);

  update({ typed: 'tsv option', cursor: { start: 10, end: 10 } });
  assert.is(        'VVVVIIIIII', statuses);
  assert.is(Status.ERROR, status);
  assert.is(0, assignC.paramIndex);
  assert.ok(assignC.getPredictions().length >= 2);
  assert.is(mockCommands.option1, assignC.getPredictions()[0].value);
  assert.is(mockCommands.option2, assignC.getPredictions()[1].value);
  assert.is('tsv', requ.commandAssignment.value.name);
  assert.is('option', assign1.arg.text);
  assert.is(undefined, assign1.value);

  update({ typed: 'tsv option', cursor: { start: 1, end: 1 } });
  assert.is(        'VVVVEEEEEE', statuses);
  assert.is(Status.ERROR, status);
  assert.is(-1, assignC.paramIndex);
  assert.is('tsv', requ.commandAssignment.value.name);
  assert.is('option', assign1.arg.text);
  assert.is(undefined, assign1.value);

  update({ typed: 'tsv option ', cursor: { start: 11, end: 11 } });
  assert.is(        'VVVVEEEEEEV', statuses);
  assert.is(Status.ERROR, status);
  assert.is(1, assignC.paramIndex);
  assert.is(0, assignC.getPredictions().length);
  assert.is('tsv', requ.commandAssignment.value.name);
  assert.is('option', assign1.arg.text);
  assert.is(undefined, assign1.value);

  update({ typed: 'tsv option1', cursor: { start: 11, end: 11 } });
  assert.is(        'VVVVVVVVVVV', statuses);
  assert.is(Status.ERROR, status);
  assert.is('tsv', requ.commandAssignment.value.name);
  assert.is('option1', assign1.arg.text);
  assert.is(mockCommands.option1, assign1.value);
  assert.is(0, assignC.paramIndex);

  update({ typed: 'tsv option1 ', cursor: { start: 12, end: 12 } });
  assert.is(        'VVVVVVVVVVVV', statuses);
  assert.is(Status.ERROR, status);
  assert.is('tsv', requ.commandAssignment.value.name);
  assert.is('option1', assign1.arg.text);
  assert.is(mockCommands.option1, assign1.value);
  assert.is(1, assignC.paramIndex);

  update({ typed: 'tsv option1 6', cursor: { start: 13, end: 13 } });
  assert.is(        'VVVVVVVVVVVVV', statuses);
  assert.is(Status.VALID, status);
  assert.is('tsv', requ.commandAssignment.value.name);
  assert.is('option1', assign1.arg.text);
  assert.is(mockCommands.option1, assign1.value);
  assert.is('6', assign2.arg.text);
  assert.is('6', assign2.value);
  assert.is('string', typeof assign2.value);
  assert.is(1, assignC.paramIndex);

  update({ typed: 'tsv option2 6', cursor: { start: 13, end: 13 } });
  assert.is(        'VVVVVVVVVVVVV', statuses);
  assert.is(Status.VALID, status);
  assert.is('tsv', requ.commandAssignment.value.name);
  assert.is('option2', assign1.arg.text);
  assert.is(mockCommands.option2, assign1.value);
  assert.is('6', assign2.arg.text);
  assert.is(6, assign2.value);
  assert.is('number', typeof assign2.value);
  assert.is(1, assignC.paramIndex);
};

exports.testInvalid = function() {
  update({ typed: 'zxjq', cursor: { start: 4, end: 4 } });
  assert.is(        'EEEE', statuses);
  assert.is('zxjq', requ.commandAssignment.arg.text);
  assert.is(0, requ._unassigned.length);
  assert.is(-1, assignC.paramIndex);

  update({ typed: 'zxjq ', cursor: { start: 5, end: 5 } });
  assert.is(        'EEEEV', statuses);
  assert.is('zxjq', requ.commandAssignment.arg.text);
  assert.is(0, requ._unassigned.length);
  assert.is(-1, assignC.paramIndex);

  update({ typed: 'zxjq one', cursor: { start: 8, end: 8 } });
  assert.is(        'EEEEVEEE', statuses);
  assert.is('zxjq', requ.commandAssignment.arg.text);
  assert.is(1, requ._unassigned.length);
  assert.is('one', requ._unassigned[0].arg.text);
};

exports.testSingleString = function() {
  update({ typed: 'tsr', cursor: { start: 3, end: 3 } });
  assert.is(        'VVV', statuses);
  assert.is(Status.ERROR, status);
  assert.is('tsr', requ.commandAssignment.value.name);
  assert.ok(assign1.arg.type === 'BlankArgument');
  assert.is(undefined, assign1.value);
  assert.is(undefined, assign2);

  update({ typed: 'tsr ', cursor: { start: 4, end: 4 } });
  assert.is(        'VVVV', statuses);
  assert.is(Status.ERROR, status);
  assert.is('tsr', requ.commandAssignment.value.name);
  assert.ok(assign1.arg.type === 'BlankArgument');
  assert.is(undefined, assign1.value);
  assert.is(undefined, assign2);

  update({ typed: 'tsr h', cursor: { start: 5, end: 5 } });
  assert.is(        'VVVVV', statuses);
  assert.is(Status.VALID, status);
  assert.is('tsr', requ.commandAssignment.value.name);
  assert.is('h', assign1.arg.text);
  assert.is('h', assign1.value);

  update({ typed: 'tsr "h h"', cursor: { start: 9, end: 9 } });
  assert.is(        'VVVVVVVVV', statuses);
  assert.is(Status.VALID, status);
  assert.is('tsr', requ.commandAssignment.value.name);
  assert.is('h h', assign1.arg.text);
  assert.is('h h', assign1.value);

  update({ typed: 'tsr h h h', cursor: { start: 9, end: 9 } });
  assert.is(        'VVVVVVVVV', statuses);
  assert.is('tsr', requ.commandAssignment.value.name);
  assert.is('h h h', assign1.arg.text);
  assert.is('h h h', assign1.value);
};

exports.testSingleNumber = function() {
  update({ typed: 'tsu', cursor: { start: 3, end: 3 } });
  assert.is(        'VVV', statuses);
  assert.is(Status.ERROR, status);
  assert.is('tsu', requ.commandAssignment.value.name);
  assert.is('', assign1.arg.text);
  assert.is(undefined, assign1.value);

  update({ typed: 'tsu ', cursor: { start: 4, end: 4 } });
  assert.is(        'VVVV', statuses);
  assert.is(Status.ERROR, status);
  assert.is('tsu', requ.commandAssignment.value.name);
  assert.is('', assign1.arg.text);
  assert.is(undefined, assign1.value);

  update({ typed: 'tsu 1', cursor: { start: 5, end: 5 } });
  assert.is(        'VVVVV', statuses);
  assert.is(Status.VALID, status);
  assert.is('tsu', requ.commandAssignment.value.name);
  assert.is('1', assign1.arg.text);
  assert.is(1, assign1.value);
  assert.is('number', typeof assign1.value);

  update({ typed: 'tsu x', cursor: { start: 5, end: 5 } });
  assert.is(        'VVVVE', statuses);
  assert.is(Status.ERROR, status);
  assert.is('tsu', requ.commandAssignment.value.name);
  assert.is('x', assign1.arg.text);
  assert.is(undefined, assign1.value);
};

exports.testElement = function(options) {
  update({ typed: 'tse', cursor: { start: 3, end: 3 } });
  assert.is(        'VVV', statuses);
  assert.is(Status.ERROR, status);
  assert.is('tse', requ.commandAssignment.value.name);
  assert.ok(assign1.arg.type === 'BlankArgument');
  assert.is(undefined, assign1.value);

  if (!options.isJsdom) {
    update({ typed: 'tse :root', cursor: { start: 9, end: 9 } });
    assert.is(        'VVVVVVVVV', statuses);
    assert.is(Status.VALID, status);
    assert.is('tse', requ.commandAssignment.value.name);
    assert.is(':root', assign1.arg.text);
    assert.is(options.window.document.documentElement, assign1.value);

    var inputElement = options.window.document.getElementById('gcli-input');
    if (inputElement) {
      update({ typed: 'tse #gcli-input', cursor: { start: 15, end: 15 } });
      assert.is(        'VVVVVVVVVVVVVVV', statuses);
      assert.is(Status.VALID, status);
      assert.is('tse', requ.commandAssignment.value.name);
      assert.is('#gcli-input', assign1.arg.text);
      assert.is(inputElement, assign1.value);
    }
    else {
      assert.log('Skipping test that assumes gcli on the web');
    }

    update({ typed: 'tse #gcli-nomatch', cursor: { start: 17, end: 17 } });
    // This is somewhat debatable because this input can't be corrected simply
    // by typing so it's and error rather than incomplete, however without
    // digging into the CSS engine we can't tell that so we default to incomplete
    assert.is(        'VVVVIIIIIIIIIIIII', statuses);
    assert.is(Status.ERROR, status);
    assert.is('tse', requ.commandAssignment.value.name);
    assert.is('#gcli-nomatch', assign1.arg.text);
    assert.is(undefined, assign1.value);
  }
  else {
    assert.log('Skipping :root test due to jsdom');
  }

  update({ typed: 'tse #', cursor: { start: 5, end: 5 } });
  assert.is(        'VVVVE', statuses);
  assert.is(Status.ERROR, status);
  assert.is('tse', requ.commandAssignment.value.name);
  assert.is('#', assign1.arg.text);
  assert.is(undefined, assign1.value);

  update({ typed: 'tse .', cursor: { start: 5, end: 5 } });
  assert.is(        'VVVVE', statuses);
  assert.is(Status.ERROR, status);
  assert.is('tse', requ.commandAssignment.value.name);
  assert.is('.', assign1.arg.text);
  assert.is(undefined, assign1.value);

  update({ typed: 'tse *', cursor: { start: 5, end: 5 } });
  assert.is(        'VVVVE', statuses);
  assert.is(Status.ERROR, status);
  assert.is('tse', requ.commandAssignment.value.name);
  assert.is('*', assign1.arg.text);
  assert.is(undefined, assign1.value);
};

exports.testNestedCommand = function() {
  update({ typed: 'tsn', cursor: { start: 3, end: 3 } });
  assert.is(        'III', statuses);
  assert.is(Status.ERROR, status);
  assert.is('tsn', requ.commandAssignment.arg.text);
  assert.is(undefined, assign1);

  update({ typed: 'tsn ', cursor: { start: 4, end: 4 } });
  assert.is(        'IIIV', statuses);
  assert.is(Status.ERROR, status);
  assert.is('tsn', requ.commandAssignment.arg.text);
  assert.is(undefined, assign1);

  update({ typed: 'tsn x', cursor: { start: 5, end: 5 } });
  // Commented out while we try out fuzzy matching
  // test.is(        'EEEVE', statuses);
  assert.is(Status.ERROR, status);
  assert.is('tsn x', requ.commandAssignment.arg.text);
  assert.is(undefined, assign1);

  update({ typed: 'tsn dif', cursor: { start: 7, end: 7 } });
  assert.is(        'VVVVVVV', statuses);
  assert.is(Status.ERROR, status);
  assert.is('tsn dif', requ.commandAssignment.value.name);
  assert.is('', assign1.arg.text);
  assert.is(undefined, assign1.value);

  update({ typed: 'tsn dif ', cursor: { start: 8, end: 8 } });
  assert.is(        'VVVVVVVV', statuses);
  assert.is(Status.ERROR, status);
  assert.is('tsn dif', requ.commandAssignment.value.name);
  assert.is('', assign1.arg.text);
  assert.is(undefined, assign1.value);

  update({ typed: 'tsn dif x', cursor: { start: 9, end: 9 } });
  assert.is(        'VVVVVVVVV', statuses);
  assert.is(Status.VALID, status);
  assert.is('tsn dif', requ.commandAssignment.value.name);
  assert.is('x', assign1.arg.text);
  assert.is('x', assign1.value);

  update({ typed: 'tsn ext', cursor: { start: 7, end: 7 } });
  assert.is(        'VVVVVVV', statuses);
  assert.is(Status.ERROR, status);
  assert.is('tsn ext', requ.commandAssignment.value.name);
  assert.is('', assign1.arg.text);
  assert.is(undefined, assign1.value);

  update({ typed: 'tsn exte', cursor: { start: 8, end: 8 } });
  assert.is(        'VVVVVVVV', statuses);
  assert.is(Status.ERROR, status);
  assert.is('tsn exte', requ.commandAssignment.value.name);
  assert.is('', assign1.arg.text);
  assert.is(undefined, assign1.value);

  update({ typed: 'tsn exten', cursor: { start: 9, end: 9 } });
  assert.is(        'VVVVVVVVV', statuses);
  assert.is(Status.ERROR, status);
  assert.is('tsn exten', requ.commandAssignment.value.name);
  assert.is('', assign1.arg.text);
  assert.is(undefined, assign1.value);

  update({ typed: 'tsn extend', cursor: { start: 10, end: 10 } });
  assert.is(        'VVVVVVVVVV', statuses);
  assert.is(Status.ERROR, status);
  assert.is('tsn extend', requ.commandAssignment.value.name);
  assert.is('', assign1.arg.text);
  assert.is(undefined, assign1.value);

  update({ typed: 'ts ', cursor: { start: 3, end: 3 } });
  assert.is(        'EEV', statuses);
  assert.is(Status.ERROR, status);
  assert.is('ts', requ.commandAssignment.arg.text);
  assert.is(undefined, assign1);
};

// From Bug 664203
exports.testDeeplyNested = function() {
  update({ typed: 'tsn deep down nested cmd', cursor: { start: 24, end: 24 } });
  assert.is(        'VVVVVVVVVVVVVVVVVVVVVVVV', statuses);
  assert.is(Status.VALID, status);
  assert.is('tsn deep down nested cmd', requ.commandAssignment.value.name);
  assert.is(undefined, assign1);

  update({ typed: 'tsn deep down nested', cursor: { start: 20, end: 20 } });
  assert.is(        'IIIVIIIIVIIIIVIIIIII', statuses);
  assert.is(Status.ERROR, status);
  assert.is('tsn deep down nested', requ.commandAssignment.value.name);
  assert.is(undefined, assign1);
};


// });
