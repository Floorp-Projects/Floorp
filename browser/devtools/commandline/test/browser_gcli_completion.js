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

const TEST_URI = "data:text/html;charset=utf-8,<p id='gcli-input'>gcli-testCompletion.js</p>";

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


// var test = require('test/assert');
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

exports.testActivate = function(options) {
  if (!options.display) {
    test.log('No display. Skipping activate tests');
    return;
  }

  helpers.setInput('');
  helpers.check({
    hints: ''
  });

  helpers.setInput(' ');
  helpers.check({
    hints: ''
  });

  helpers.setInput('tsr');
  helpers.check({
    hints: ' <text>'
  });

  helpers.setInput('tsr ');
  helpers.check({
    hints: '<text>'
  });

  helpers.setInput('tsr b');
  helpers.check({
    hints: ''
  });

  helpers.setInput('tsb');
  helpers.check({
    hints: ' [toggle]'
  });

  helpers.setInput('tsm');
  helpers.check({
    hints: ' <abc> <txt> <num>'
  });

  helpers.setInput('tsm ');
  helpers.check({
    hints: 'a <txt> <num>'
  });

  helpers.setInput('tsm a');
  helpers.check({
    hints: ' <txt> <num>'
  });

  helpers.setInput('tsm a ');
  helpers.check({
    hints: '<txt> <num>'
  });

  helpers.setInput('tsm a  ');
  helpers.check({
    hints: '<txt> <num>'
  });

  helpers.setInput('tsm a  d');
  helpers.check({
    hints: ' <num>'
  });

  helpers.setInput('tsm a "d d"');
  helpers.check({
    hints: ' <num>'
  });

  helpers.setInput('tsm a "d ');
  helpers.check({
    hints: ' <num>'
  });

  helpers.setInput('tsm a "d d" ');
  helpers.check({
    hints: '<num>'
  });

  helpers.setInput('tsm a "d d ');
  helpers.check({
    hints: ' <num>'
  });

  helpers.setInput('tsm d r');
  helpers.check({
    hints: ' <num>'
  });

  helpers.setInput('tsm a d ');
  helpers.check({
    hints: '<num>'
  });

  helpers.setInput('tsm a d 4');
  helpers.check({
    hints: ''
  });

  helpers.setInput('tsg');
  helpers.check({
    hints: ' <solo> [options]'
  });

  helpers.setInput('tsg ');
  helpers.check({
    hints: 'aaa [options]'
  });

  helpers.setInput('tsg a');
  helpers.check({
    hints: 'aa [options]'
  });

  helpers.setInput('tsg b');
  helpers.check({
    hints: 'bb [options]'
  });

  helpers.setInput('tsg d');
  helpers.check({
    hints: ' [options] -> ccc'
  });

  helpers.setInput('tsg aa');
  helpers.check({
    hints: 'a [options]'
  });

  helpers.setInput('tsg aaa');
  helpers.check({
    hints: ' [options]'
  });

  helpers.setInput('tsg aaa ');
  helpers.check({
    hints: '[options]'
  });

  helpers.setInput('tsg aaa d');
  helpers.check({
    hints: ' [options]'
  });

  helpers.setInput('tsg aaa dddddd');
  helpers.check({
    hints: ' [options]'
  });

  helpers.setInput('tsg aaa dddddd ');
  helpers.check({
    hints: '[options]'
  });

  helpers.setInput('tsg aaa "d');
  helpers.check({
    hints: ' [options]'
  });

  helpers.setInput('tsg aaa "d d');
  helpers.check({
    hints: ' [options]'
  });

  helpers.setInput('tsg aaa "d d"');
  helpers.check({
    hints: ' [options]'
  });

  helpers.setInput('tsn ex ');
  helpers.check({
    hints: ''
  });

  helpers.setInput('selarr');
  helpers.check({
    hints: ' -> tselarr'
  });

  helpers.setInput('tselar 1');
  helpers.check({
    hints: ''
  });

  helpers.setInput('tselar 1', 7);
  helpers.check({
    hints: ''
  });

  helpers.setInput('tselar 1', 6);
  helpers.check({
    hints: ' -> tselarr'
  });

  helpers.setInput('tselar 1', 5);
  helpers.check({
    hints: ' -> tselarr'
  });
};

exports.testLong = function(options) {
  helpers.setInput('tslong --sel');
  helpers.check({
    input:  'tslong --sel',
    hints:              ' <selection> <msg> [options]',
    markup: 'VVVVVVVIIIII'
  });

  helpers.pressTab();
  helpers.check({
    input:  'tslong --sel ',
    hints:               'space <msg> [options]',
    markup: 'VVVVVVVIIIIIV'
  });

  helpers.setInput('tslong --sel ');
  helpers.check({
    input:  'tslong --sel ',
    hints:               'space <msg> [options]',
    markup: 'VVVVVVVIIIIIV'
  });

  helpers.setInput('tslong --sel s');
  helpers.check({
    input:  'tslong --sel s',
    hints:                'pace <msg> [options]',
    markup: 'VVVVVVVIIIIIVI'
  });

  helpers.setInput('tslong --num ');
  helpers.check({
    input:  'tslong --num ',
    hints:               '<number> <msg> [options]',
    markup: 'VVVVVVVIIIIIV'
  });

  helpers.setInput('tslong --num 42');
  helpers.check({
    input:  'tslong --num 42',
    hints:                 ' <msg> [options]',
    markup: 'VVVVVVVVVVVVVVV'
  });

  helpers.setInput('tslong --num 42 ');
  helpers.check({
    input:  'tslong --num 42 ',
    hints:                  '<msg> [options]',
    markup: 'VVVVVVVVVVVVVVVV'
  });

  helpers.setInput('tslong --num 42 --se');
  helpers.check({
    input:  'tslong --num 42 --se',
    hints:                      'l <msg> [options]',
    markup: 'VVVVVVVVVVVVVVVVIIII'
  });

  helpers.pressTab();
  helpers.check({
    input:  'tslong --num 42 --sel ',
    hints:                        'space <msg> [options]',
    markup: 'VVVVVVVVVVVVVVVVIIIIIV'
  });

  helpers.pressTab();
  helpers.check({
    input:  'tslong --num 42 --sel space ',
    hints:                              '<msg> [options]',
    markup: 'VVVVVVVVVVVVVVVVVVVVVVVVVVVV'
  });

  helpers.setInput('tslong --num 42 --sel ');
  helpers.check({
    input:  'tslong --num 42 --sel ',
    hints:                        'space <msg> [options]',
    markup: 'VVVVVVVVVVVVVVVVIIIIIV'
  });

  helpers.setInput('tslong --num 42 --sel space ');
  helpers.check({
    input:  'tslong --num 42 --sel space ',
    hints:                              '<msg> [options]',
    markup: 'VVVVVVVVVVVVVVVVVVVVVVVVVVVV'
  });
};

exports.testNoTab = function(options) {
  helpers.setInput('tss');
  helpers.pressTab();
  helpers.check({
    input:  'tss ',
    markup: 'VVVV',
    hints: ''
  });

  helpers.pressTab();
  helpers.check({
    input:  'tss ',
    markup: 'VVVV',
    hints: ''
  });

  helpers.setInput('xxxx');
  helpers.check({
    input:  'xxxx',
    markup: 'EEEE',
    hints: ''
  });

  helpers.pressTab();
  helpers.check({
    input:  'xxxx',
    markup: 'EEEE',
    hints: ''
  });
};

exports.testOutstanding = function(options) {
  // See bug 779800
  /*
  helpers.setInput('tsg --txt1 ddd ');
  helpers.check({
    input:  'tsg --txt1 ddd ',
    hints:                 'aaa [options]',
    markup: 'VVVVVVVVVVVVVVV'
  });
  */
};

exports.testCompleteIntoOptional = function(options) {
  // From bug 779816
  helpers.setInput('tso ');
  helpers.check({
    typed:  'tso ',
    hints:      '[text]',
    markup: 'VVVV',
    status: 'VALID'
  });

  helpers.setInput('tso');
  helpers.pressTab();
  helpers.check({
    typed:  'tso ',
    hints:      '[text]',
    markup: 'VVVV',
    status: 'VALID'
  });
};


// });
