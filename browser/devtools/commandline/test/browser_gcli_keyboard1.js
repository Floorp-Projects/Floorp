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

var TEST_URI = "data:text/html;charset=utf-8,<p id='gcli-input'>gcli-testKeyboard1.js</p>";

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

var javascript = require('gcli/types/javascript');
// var helpers = require('./helpers');

var tempWindow;

exports.setup = function(options) {
  tempWindow = javascript.getGlobalObject();
  javascript.setGlobalObject(options.window);
};

exports.shutdown = function(options) {
  javascript.setGlobalObject(tempWindow);
  tempWindow = undefined;
};

exports.testSimple = function(options) {
  return helpers.audit(options, [
    {
      setup: 'tsela<TAB>',
      check: { input: 'tselarr ', cursor: 8 }
    },
    {
      setup: 'tsn di<TAB>',
      check: { input: 'tsn dif ', cursor: 8 }
    },
    {
      setup: 'tsg a<TAB>',
      check: { input: 'tsg aaa ', cursor: 8 }
    }
  ]);
};

exports.testScript = function(options) {
  return helpers.audit(options, [
    {
      skipIf: function commandJsMissing() {
        return options.requisition.canon.getCommand('{') == null;
      },
      setup: '{ wind<TAB>',
      check: { input: '{ window' }
    },
    {
      skipIf: function commandJsMissing() {
        return options.requisition.canon.getCommand('{') == null;
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
        return options.requisition.canon.getCommand('{') == null;
      },
      setup: '{ window.document.titl<TAB>',
      check: { input: '{ window.document.title ' }
    }
  ]);
};
