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

// THIS FILE IS GENERATED FROM SOURCE IN THE GCLI PROJECT
// PLEASE TALK TO SOMEONE IN DEVELOPER TOOLS BEFORE EDITING IT

const exports = {};

function test() {
  helpers.runTestModule(exports, "browser_gcli_keyboard1.js");
}

var javascript = require('gcli/types/javascript');
// var helpers = require('./helpers');

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
      skipRemainingIf: options.isRemote ||
              options.requisition.system.commands.get('{') == null,
      setup: '{ wind<TAB>',
      check: { input: '{ window' }
    },
    {
      setup: '{ window.docum<TAB>',
      check: { input: '{ window.document' }
    }
  ]);
};

exports.testJsdom = function(options) {
  return helpers.audit(options, [
    {
      skipIf: options.isRemote ||
              options.requisition.system.commands.get('{') == null,
      setup: '{ window.document.titl<TAB>',
      check: { input: '{ window.document.title ' }
    }
  ]);
};
