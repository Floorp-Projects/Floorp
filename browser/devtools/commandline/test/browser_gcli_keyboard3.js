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

const TEST_URI = "data:text/html;charset=utf-8,<p id='gcli-input'>gcli-testKeyboard3.js</p>";

function test() {
  helpers.addTabWithToolbar(TEST_URI, function(options) {
    return helpers.runTests(options, exports);
  }).then(finish);
}

// <INJECTED SOURCE:END>

'use strict';

var javascript = require('gcli/types/javascript');
// var helpers = require('gclitest/helpers');
// var mockCommands = require('gclitest/mockCommands');
var canon = require('gcli/canon');

var tempWindow;

exports.setup = function(options) {
  mockCommands.setup();

  tempWindow = javascript.getGlobalObject();
  javascript.setGlobalObject(options.window);
};

exports.shutdown = function(options) {
  javascript.setGlobalObject(tempWindow);
  tempWindow = undefined;

  mockCommands.shutdown();
};

exports.testScript = function(options) {
  return helpers.audit(options, [
    {
      skipIf: function commandJsMissing() {
        return canon.getCommand('{') == null;
      },
      setup: '{ wind<TAB>',
      check: { input: '{ window' }
    },
    {
      skipIf: function commandJsMissing() {
        return canon.getCommand('{') == null;
      },
      setup: '{ window.docum<TAB>',
      check: { input: '{ window.document' }
    }
  ]);
};

exports.testJsdom = function(options) {
  return helpers.audit(options, [
    {
      skipIf: function jsDomOrCommandJsMissing() {
        return options.isJsdom || canon.getCommand('{') == null;
      },
      setup: '{ window.document.titl<TAB>',
      check: { input: '{ window.document.title ' }
    }
  ]);
};


// });
