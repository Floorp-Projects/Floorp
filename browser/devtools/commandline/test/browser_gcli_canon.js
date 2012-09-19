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

const TEST_URI = "data:text/html;charset=utf-8,<p id='gcli-input'>gcli-testCanon.js</p>";

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

  // var helpers = require('gclitest/helpers');
  var canon = require('gcli/canon');
  // var assert = require('test/assert');

  exports.setup = function(options) {
    helpers.setup(options);
  };

  exports.shutdown = function(options) {
    helpers.shutdown(options);
  };

  exports.testAddRemove = function(options) {
    var startCount = canon.getCommands().length;
    var events = 0;

    var canonChange = function(ev) {
      events++;
    };
    canon.onCanonChange.add(canonChange);

    canon.addCommand({
      name: 'testadd',
      exec: function() {
        return 1;
      }
    });
    assert.is(canon.getCommands().length,
              startCount + 1,
              'add command success');
    assert.is(events, 1, 'add event');
    helpers.exec({
      typed: 'testadd',
      outputMatch: /^1$/
    });

    canon.addCommand({
      name: 'testadd',
      exec: function() {
        return 2;
      }
    });

    assert.is(canon.getCommands().length,
              startCount + 1,
              'readd command success');
    assert.is(events, 2, 'readd event');
    helpers.exec({
      typed: 'testadd',
      outputMatch: /^2$/
    });

    canon.removeCommand('testadd');

    assert.is(canon.getCommands().length,
              startCount,
              'remove command success');
    assert.is(events, 3, 'remove event');

    helpers.setInput('testadd');
    helpers.check({
      typed: 'testadd',
      status: 'ERROR'
    });

    canon.addCommand({
      name: 'testadd',
      exec: function() {
        return 3;
      }
    });

    assert.is(canon.getCommands().length,
              startCount + 1,
              'rereadd command success');
    assert.is(events, 4, 'rereadd event');
    helpers.exec({
      typed: 'testadd',
      outputMatch: /^3$/
    });

    canon.removeCommand({
      name: 'testadd'
    });

    assert.is(canon.getCommands().length,
              startCount,
              'reremove command success');
    assert.is(events, 5, 'reremove event');

    helpers.setInput('testadd');
    helpers.check({
      typed: 'testadd',
      status: 'ERROR'
    });

    canon.removeCommand({ name: 'nonexistant' });
    assert.is(canon.getCommands().length,
              startCount,
              'nonexistant1 command success');
    assert.is(events, 5, 'nonexistant1 event');

    canon.removeCommand('nonexistant');
    assert.is(canon.getCommands().length,
              startCount,
              'nonexistant2 command success');
    assert.is(events, 5, 'nonexistant2 event');

    canon.onCanonChange.remove(canonChange);
  };

// });
