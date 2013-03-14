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

const TEST_URI = "data:text/html;charset=utf-8,<p id='gcli-input'>gcli-testUtil.js</p>";

function test() {
  helpers.addTabWithToolbar(TEST_URI, function(options) {
    return helpers.runTests(options, exports);
  }).then(finish);
}

// <INJECTED SOURCE:END>

'use strict';

var util = require('util/util');
// var assert = require('test/assert');

exports.testFindCssSelector = function(options) {
  var nodes = options.window.document.querySelectorAll('*');
  for (var i = 0; i < nodes.length; i++) {
    var selector = util.findCssSelector(nodes[i]);
    var matches = options.window.document.querySelectorAll(selector);

    assert.is(matches.length, 1, 'multiple matches for ' + selector);
    assert.is(matches[0], nodes[i], 'non-matching selector: ' + selector);
  }
};


// });
