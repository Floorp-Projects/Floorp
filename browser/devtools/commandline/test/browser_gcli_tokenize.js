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

var TEST_URI = "data:text/html;charset=utf-8,<p id='gcli-input'>gcli-testTokenize.js</p>";

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
var cli = require('gcli/cli');

exports.testBlanks = function(options) {
  var args;

  args = cli.tokenize('');
  assert.is(args.length, 1);
  assert.is(args[0].text, '');
  assert.is(args[0].prefix, '');
  assert.is(args[0].suffix, '');

  args = cli.tokenize(' ');
  assert.is(args.length, 1);
  assert.is(args[0].text, '');
  assert.is(args[0].prefix, ' ');
  assert.is(args[0].suffix, '');
};

exports.testTokSimple = function(options) {
  var args;

  args = cli.tokenize('s');
  assert.is(args.length, 1);
  assert.is(args[0].text, 's');
  assert.is(args[0].prefix, '');
  assert.is(args[0].suffix, '');
  assert.is(args[0].type, 'Argument');

  args = cli.tokenize('s s');
  assert.is(args.length, 2);
  assert.is(args[0].text, 's');
  assert.is(args[0].prefix, '');
  assert.is(args[0].suffix, '');
  assert.is(args[0].type, 'Argument');
  assert.is(args[1].text, 's');
  assert.is(args[1].prefix, ' ');
  assert.is(args[1].suffix, '');
  assert.is(args[1].type, 'Argument');
};

exports.testJavascript = function(options) {
  var args;

  args = cli.tokenize('{x}');
  assert.is(args.length, 1);
  assert.is(args[0].text, 'x');
  assert.is(args[0].prefix, '{');
  assert.is(args[0].suffix, '}');
  assert.is(args[0].type, 'ScriptArgument');

  args = cli.tokenize('{ x }');
  assert.is(args.length, 1);
  assert.is(args[0].text, 'x');
  assert.is(args[0].prefix, '{ ');
  assert.is(args[0].suffix, ' }');
  assert.is(args[0].type, 'ScriptArgument');

  args = cli.tokenize('{x} {y}');
  assert.is(args.length, 2);
  assert.is(args[0].text, 'x');
  assert.is(args[0].prefix, '{');
  assert.is(args[0].suffix, '}');
  assert.is(args[0].type, 'ScriptArgument');
  assert.is(args[1].text, 'y');
  assert.is(args[1].prefix, ' {');
  assert.is(args[1].suffix, '}');
  assert.is(args[1].type, 'ScriptArgument');

  args = cli.tokenize('{x}{y}');
  assert.is(args.length, 2);
  assert.is(args[0].text, 'x');
  assert.is(args[0].prefix, '{');
  assert.is(args[0].suffix, '}');
  assert.is(args[0].type, 'ScriptArgument');
  assert.is(args[1].text, 'y');
  assert.is(args[1].prefix, '{');
  assert.is(args[1].suffix, '}');
  assert.is(args[1].type, 'ScriptArgument');

  args = cli.tokenize('{');
  assert.is(args.length, 1);
  assert.is(args[0].text, '');
  assert.is(args[0].prefix, '{');
  assert.is(args[0].suffix, '');
  assert.is(args[0].type, 'ScriptArgument');

  args = cli.tokenize('{ ');
  assert.is(args.length, 1);
  assert.is(args[0].text, '');
  assert.is(args[0].prefix, '{ ');
  assert.is(args[0].suffix, '');
  assert.is(args[0].type, 'ScriptArgument');

  args = cli.tokenize('{x');
  assert.is(args.length, 1);
  assert.is(args[0].text, 'x');
  assert.is(args[0].prefix, '{');
  assert.is(args[0].suffix, '');
  assert.is(args[0].type, 'ScriptArgument');
};

exports.testRegularNesting = function(options) {
  var args;

  args = cli.tokenize('{"x"}');
  assert.is(args.length, 1);
  assert.is(args[0].text, '"x"');
  assert.is(args[0].prefix, '{');
  assert.is(args[0].suffix, '}');
  assert.is(args[0].type, 'ScriptArgument');

  args = cli.tokenize('{\'x\'}');
  assert.is(args.length, 1);
  assert.is(args[0].text, '\'x\'');
  assert.is(args[0].prefix, '{');
  assert.is(args[0].suffix, '}');
  assert.is(args[0].type, 'ScriptArgument');

  args = cli.tokenize('"{x}"');
  assert.is(args.length, 1);
  assert.is(args[0].text, '{x}');
  assert.is(args[0].prefix, '"');
  assert.is(args[0].suffix, '"');
  assert.is(args[0].type, 'Argument');

  args = cli.tokenize('\'{x}\'');
  assert.is(args.length, 1);
  assert.is(args[0].text, '{x}');
  assert.is(args[0].prefix, '\'');
  assert.is(args[0].suffix, '\'');
  assert.is(args[0].type, 'Argument');
};

exports.testDeepNesting = function(options) {
  var args;

  args = cli.tokenize('{{}}');
  assert.is(args.length, 1);
  assert.is(args[0].text, '{}');
  assert.is(args[0].prefix, '{');
  assert.is(args[0].suffix, '}');
  assert.is(args[0].type, 'ScriptArgument');

  args = cli.tokenize('{{x} {y}}');
  assert.is(args.length, 1);
  assert.is(args[0].text, '{x} {y}');
  assert.is(args[0].prefix, '{');
  assert.is(args[0].suffix, '}');
  assert.is(args[0].type, 'ScriptArgument');

  args = cli.tokenize('{{w} {{{x}}}} {y} {{{z}}}');

  assert.is(args.length, 3);

  assert.is(args[0].text, '{w} {{{x}}}');
  assert.is(args[0].prefix, '{');
  assert.is(args[0].suffix, '}');
  assert.is(args[0].type, 'ScriptArgument');

  assert.is(args[1].text, 'y');
  assert.is(args[1].prefix, ' {');
  assert.is(args[1].suffix, '}');
  assert.is(args[1].type, 'ScriptArgument');

  assert.is(args[2].text, '{{z}}');
  assert.is(args[2].prefix, ' {');
  assert.is(args[2].suffix, '}');
  assert.is(args[2].type, 'ScriptArgument');

  args = cli.tokenize('{{w} {{{x}}} {y} {{{z}}}');

  assert.is(args.length, 1);

  assert.is(args[0].text, '{w} {{{x}}} {y} {{{z}}}');
  assert.is(args[0].prefix, '{');
  assert.is(args[0].suffix, '');
  assert.is(args[0].type, 'ScriptArgument');
};

exports.testStrangeNesting = function(options) {
  var args;

  // Note: When we get real JS parsing this should break
  args = cli.tokenize('{"x}"}');

  assert.is(args.length, 2);

  assert.is(args[0].text, '"x');
  assert.is(args[0].prefix, '{');
  assert.is(args[0].suffix, '}');
  assert.is(args[0].type, 'ScriptArgument');

  assert.is(args[1].text, '}');
  assert.is(args[1].prefix, '"');
  assert.is(args[1].suffix, '');
  assert.is(args[1].type, 'Argument');
};

exports.testComplex = function(options) {
  var args;

  args = cli.tokenize(' 1234  \'12 34\'');

  assert.is(args.length, 2);

  assert.is(args[0].text, '1234');
  assert.is(args[0].prefix, ' ');
  assert.is(args[0].suffix, '');
  assert.is(args[0].type, 'Argument');

  assert.is(args[1].text, '12 34');
  assert.is(args[1].prefix, '  \'');
  assert.is(args[1].suffix, '\'');
  assert.is(args[1].type, 'Argument');

  args = cli.tokenize('12\'34 "12 34" \\'); // 12'34 "12 34" \

  assert.is(args.length, 3);

  assert.is(args[0].text, '12\'34');
  assert.is(args[0].prefix, '');
  assert.is(args[0].suffix, '');
  assert.is(args[0].type, 'Argument');

  assert.is(args[1].text, '12 34');
  assert.is(args[1].prefix, ' "');
  assert.is(args[1].suffix, '"');
  assert.is(args[1].type, 'Argument');

  assert.is(args[2].text, '\\');
  assert.is(args[2].prefix, ' ');
  assert.is(args[2].suffix, '');
  assert.is(args[2].type, 'Argument');
};

exports.testPathological = function(options) {
  var args;

  args = cli.tokenize('a\\ b \\t\\n\\r \\\'x\\\" \'d'); // a_b \t\n\r \'x\" 'd

  assert.is(args.length, 4);

  assert.is(args[0].text, 'a\\ b');
  assert.is(args[0].prefix, '');
  assert.is(args[0].suffix, '');
  assert.is(args[0].type, 'Argument');

  assert.is(args[1].text, '\\t\\n\\r');
  assert.is(args[1].prefix, ' ');
  assert.is(args[1].suffix, '');
  assert.is(args[1].type, 'Argument');

  assert.is(args[2].text, '\\\'x\\"');
  assert.is(args[2].prefix, ' ');
  assert.is(args[2].suffix, '');
  assert.is(args[2].type, 'Argument');

  assert.is(args[3].text, 'd');
  assert.is(args[3].prefix, ' \'');
  assert.is(args[3].suffix, '');
  assert.is(args[3].type, 'Argument');
};
