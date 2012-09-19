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

const TEST_URI = "data:text/html;charset=utf-8,<p id='gcli-input'>gcli-testTokenize.js</p>";

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
var Requisition = require('gcli/cli').Requisition;

exports.testBlanks = function() {
  var args;
  var requ = new Requisition();

  args = requ._tokenize('');
  assert.is(1, args.length);
  assert.is('', args[0].text);
  assert.is('', args[0].prefix);
  assert.is('', args[0].suffix);

  args = requ._tokenize(' ');
  assert.is(1, args.length);
  assert.is('', args[0].text);
  assert.is(' ', args[0].prefix);
  assert.is('', args[0].suffix);
};

exports.testTokSimple = function() {
  var args;
  var requ = new Requisition();

  args = requ._tokenize('s');
  assert.is(1, args.length);
  assert.is('s', args[0].text);
  assert.is('', args[0].prefix);
  assert.is('', args[0].suffix);
  assert.is('Argument', args[0].type);

  args = requ._tokenize('s s');
  assert.is(2, args.length);
  assert.is('s', args[0].text);
  assert.is('', args[0].prefix);
  assert.is('', args[0].suffix);
  assert.is('Argument', args[0].type);
  assert.is('s', args[1].text);
  assert.is(' ', args[1].prefix);
  assert.is('', args[1].suffix);
  assert.is('Argument', args[1].type);
};

exports.testJavascript = function() {
  var args;
  var requ = new Requisition();

  args = requ._tokenize('{x}');
  assert.is(1, args.length);
  assert.is('x', args[0].text);
  assert.is('{', args[0].prefix);
  assert.is('}', args[0].suffix);
  assert.is('ScriptArgument', args[0].type);

  args = requ._tokenize('{ x }');
  assert.is(1, args.length);
  assert.is('x', args[0].text);
  assert.is('{ ', args[0].prefix);
  assert.is(' }', args[0].suffix);
  assert.is('ScriptArgument', args[0].type);

  args = requ._tokenize('{x} {y}');
  assert.is(2, args.length);
  assert.is('x', args[0].text);
  assert.is('{', args[0].prefix);
  assert.is('}', args[0].suffix);
  assert.is('ScriptArgument', args[0].type);
  assert.is('y', args[1].text);
  assert.is(' {', args[1].prefix);
  assert.is('}', args[1].suffix);
  assert.is('ScriptArgument', args[1].type);

  args = requ._tokenize('{x}{y}');
  assert.is(2, args.length);
  assert.is('x', args[0].text);
  assert.is('{', args[0].prefix);
  assert.is('}', args[0].suffix);
  assert.is('ScriptArgument', args[0].type);
  assert.is('y', args[1].text);
  assert.is('{', args[1].prefix);
  assert.is('}', args[1].suffix);
  assert.is('ScriptArgument', args[1].type);

  args = requ._tokenize('{');
  assert.is(1, args.length);
  assert.is('', args[0].text);
  assert.is('{', args[0].prefix);
  assert.is('', args[0].suffix);
  assert.is('ScriptArgument', args[0].type);

  args = requ._tokenize('{ ');
  assert.is(1, args.length);
  assert.is('', args[0].text);
  assert.is('{ ', args[0].prefix);
  assert.is('', args[0].suffix);
  assert.is('ScriptArgument', args[0].type);

  args = requ._tokenize('{x');
  assert.is(1, args.length);
  assert.is('x', args[0].text);
  assert.is('{', args[0].prefix);
  assert.is('', args[0].suffix);
  assert.is('ScriptArgument', args[0].type);
};

exports.testRegularNesting = function() {
  var args;
  var requ = new Requisition();

  args = requ._tokenize('{"x"}');
  assert.is(1, args.length);
  assert.is('"x"', args[0].text);
  assert.is('{', args[0].prefix);
  assert.is('}', args[0].suffix);
  assert.is('ScriptArgument', args[0].type);

  args = requ._tokenize('{\'x\'}');
  assert.is(1, args.length);
  assert.is('\'x\'', args[0].text);
  assert.is('{', args[0].prefix);
  assert.is('}', args[0].suffix);
  assert.is('ScriptArgument', args[0].type);

  args = requ._tokenize('"{x}"');
  assert.is(1, args.length);
  assert.is('{x}', args[0].text);
  assert.is('"', args[0].prefix);
  assert.is('"', args[0].suffix);
  assert.is('Argument', args[0].type);

  args = requ._tokenize('\'{x}\'');
  assert.is(1, args.length);
  assert.is('{x}', args[0].text);
  assert.is('\'', args[0].prefix);
  assert.is('\'', args[0].suffix);
  assert.is('Argument', args[0].type);
};

exports.testDeepNesting = function() {
  var args;
  var requ = new Requisition();

  args = requ._tokenize('{{}}');
  assert.is(1, args.length);
  assert.is('{}', args[0].text);
  assert.is('{', args[0].prefix);
  assert.is('}', args[0].suffix);
  assert.is('ScriptArgument', args[0].type);

  args = requ._tokenize('{{x} {y}}');
  assert.is(1, args.length);
  assert.is('{x} {y}', args[0].text);
  assert.is('{', args[0].prefix);
  assert.is('}', args[0].suffix);
  assert.is('ScriptArgument', args[0].type);

  args = requ._tokenize('{{w} {{{x}}}} {y} {{{z}}}');

  assert.is(3, args.length);

  assert.is('{w} {{{x}}}', args[0].text);
  assert.is('{', args[0].prefix);
  assert.is('}', args[0].suffix);
  assert.is('ScriptArgument', args[0].type);

  assert.is('y', args[1].text);
  assert.is(' {', args[1].prefix);
  assert.is('}', args[1].suffix);
  assert.is('ScriptArgument', args[1].type);

  assert.is('{{z}}', args[2].text);
  assert.is(' {', args[2].prefix);
  assert.is('}', args[2].suffix);
  assert.is('ScriptArgument', args[2].type);

  args = requ._tokenize('{{w} {{{x}}} {y} {{{z}}}');

  assert.is(1, args.length);

  assert.is('{w} {{{x}}} {y} {{{z}}}', args[0].text);
  assert.is('{', args[0].prefix);
  assert.is('', args[0].suffix);
  assert.is('ScriptArgument', args[0].type);
};

exports.testStrangeNesting = function() {
  var args;
  var requ = new Requisition();

  // Note: When we get real JS parsing this should break
  args = requ._tokenize('{"x}"}');

  assert.is(2, args.length);

  assert.is('"x', args[0].text);
  assert.is('{', args[0].prefix);
  assert.is('}', args[0].suffix);
  assert.is('ScriptArgument', args[0].type);

  assert.is('}', args[1].text);
  assert.is('"', args[1].prefix);
  assert.is('', args[1].suffix);
  assert.is('Argument', args[1].type);
};

exports.testComplex = function() {
  var args;
  var requ = new Requisition();

  args = requ._tokenize(' 1234  \'12 34\'');

  assert.is(2, args.length);

  assert.is('1234', args[0].text);
  assert.is(' ', args[0].prefix);
  assert.is('', args[0].suffix);
  assert.is('Argument', args[0].type);

  assert.is('12 34', args[1].text);
  assert.is('  \'', args[1].prefix);
  assert.is('\'', args[1].suffix);
  assert.is('Argument', args[1].type);

  args = requ._tokenize('12\'34 "12 34" \\'); // 12'34 "12 34" \

  assert.is(3, args.length);

  assert.is('12\'34', args[0].text);
  assert.is('', args[0].prefix);
  assert.is('', args[0].suffix);
  assert.is('Argument', args[0].type);

  assert.is('12 34', args[1].text);
  assert.is(' "', args[1].prefix);
  assert.is('"', args[1].suffix);
  assert.is('Argument', args[1].type);

  assert.is('\\', args[2].text);
  assert.is(' ', args[2].prefix);
  assert.is('', args[2].suffix);
  assert.is('Argument', args[2].type);
};

exports.testPathological = function() {
  var args;
  var requ = new Requisition();

  args = requ._tokenize('a\\ b \\t\\n\\r \\\'x\\\" \'d'); // a_b \t\n\r \'x\" 'd

  assert.is(4, args.length);

  assert.is('a b', args[0].text);
  assert.is('', args[0].prefix);
  assert.is('', args[0].suffix);
  assert.is('Argument', args[0].type);

  assert.is('\t\n\r', args[1].text);
  assert.is(' ', args[1].prefix);
  assert.is('', args[1].suffix);
  assert.is('Argument', args[1].type);

  assert.is('\'x"', args[2].text);
  assert.is(' ', args[2].prefix);
  assert.is('', args[2].suffix);
  assert.is('Argument', args[2].type);

  assert.is('d', args[3].text);
  assert.is(' \'', args[3].prefix);
  assert.is('', args[3].suffix);
  assert.is('Argument', args[3].type);
};


// });
