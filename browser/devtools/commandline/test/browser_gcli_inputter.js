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

const TEST_URI = "data:text/html;charset=utf-8,<p id='gcli-input'>gcli-testInputter.js</p>";

function test() {
  helpers.addTabWithToolbar(TEST_URI, function(options) {
    return helpers.runTests(options, exports);
  }).then(finish);
}

// <INJECTED SOURCE:END>

'use strict';

var KeyEvent = require('util/util').KeyEvent;
// var assert = require('test/assert');
// var mockCommands = require('gclitest/mockCommands');

var latestEvent = undefined;
var latestData = undefined;

var outputted = function(ev) {
  latestEvent = ev;

  ev.output.promise.then(function() {
    latestData = ev.output.data;
    ev.output.onClose();
  });
};


exports.setup = function(options) {
  mockCommands.setup();
  options.display.requisition.commandOutputManager.onOutput.add(outputted);
};

exports.shutdown = function(options) {
  mockCommands.shutdown();
  options.display.requisition.commandOutputManager.onOutput.remove(outputted);
};

exports.testOutput = function(options) {
  latestEvent = undefined;
  latestData = undefined;

  var inputter = options.display.inputter;
  var focusManager = options.display.focusManager;

  inputter.setInput('tss');

  var ev0 = { keyCode: KeyEvent.DOM_VK_RETURN };
  inputter.onKeyDown(ev0);

  assert.is(inputter.element.value, 'tss', 'inputter should do nothing on RETURN keyDown');
  assert.is(latestEvent, undefined, 'no events this test');
  assert.is(latestData, undefined, 'no data this test');

  var ev1 = { keyCode: KeyEvent.DOM_VK_RETURN };
  return inputter.handleKeyUp(ev1).then(function() {
    assert.ok(latestEvent != null, 'events this test');
    assert.is(latestData, 'Exec: tss ', 'last command is tss');

    assert.is(inputter.element.value, '', 'inputter should exec on RETURN keyUp');

    assert.ok(focusManager._recentOutput, 'recent output happened');

    var ev2 = { keyCode: KeyEvent.DOM_VK_F1 };
    return inputter.handleKeyUp(ev2).then(function() {
      assert.ok(!focusManager._recentOutput, 'no recent output happened post F1');
      assert.ok(focusManager._helpRequested, 'F1 = help');

      var ev3 = { keyCode: KeyEvent.DOM_VK_ESCAPE };
      return inputter.handleKeyUp(ev3).then(function() {
        assert.ok(!focusManager._helpRequested, 'ESCAPE = anti help');
      });
    });

  });
};


// });
