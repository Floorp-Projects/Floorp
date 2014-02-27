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

var TEST_URI = "data:text/html;charset=utf-8,<p id='gcli-input'>gcli-testTypes.js</p>";

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
var util = require('gcli/util/util');
var promise = require('gcli/util/promise');
var nodetype = require('gcli/types/node');

exports.setup = function(options) {
  if (options.window) {
    nodetype.setDocument(options.window.document);
  }
};

exports.shutdown = function(options) {
  nodetype.unsetDocument();
};

function forEachType(options, typeSpec, callback) {
  var types = options.requisition.types;
  return util.promiseEach(types.getTypeNames(), function(name) {
    typeSpec.name = name;
    typeSpec.requisition = options.requisition;

    // Provide some basic defaults to help selection/delegate/array work
    if (name === 'selection') {
      typeSpec.data = [ 'a', 'b' ];
    }
    else if (name === 'delegate') {
      typeSpec.delegateType = function() {
        return 'string';
      };
    }
    else if (name === 'array') {
      typeSpec.subtype = 'string';
    }
    else if (name === 'remote') {
      return;
    }

    var type = types.createType(typeSpec);
    var reply = callback(type);
    return promise.resolve(reply).then(function(value) {
      // Clean up
      delete typeSpec.name;
      delete typeSpec.requisition;
      delete typeSpec.data;
      delete typeSpec.delegateType;
      delete typeSpec.subtype;

      return value;
    });
  });
}

exports.testDefault = function(options) {
  if (options.isNoDom) {
    assert.log('Skipping tests due to issues with resource type.');
    return;
  }

  return forEachType(options, {}, function(type) {
    var context = options.requisition.executionContext;
    var blank = type.getBlank(context).value;

    // boolean and array types are exempt from needing undefined blank values
    if (type.name === 'boolean') {
      assert.is(blank, false, 'blank boolean is false');
    }
    else if (type.name === 'array') {
      assert.ok(Array.isArray(blank), 'blank array is array');
      assert.is(blank.length, 0, 'blank array is empty');
    }
    else if (type.name === 'nodelist') {
      assert.ok(typeof blank.item, 'function', 'blank.item is function');
      assert.is(blank.length, 0, 'blank nodelist is empty');
    }
    else {
      assert.is(blank, undefined, 'default defined for ' + type.name);
    }
  });
};

exports.testNullDefault = function(options) {
  var context = null; // Is this test still valid with a null context?

  return forEachType(options, { defaultValue: null }, function(type) {
    var reply = type.stringify(null, context);
    return promise.resolve(reply).then(function(str) {
      assert.is(str, '', 'stringify(null) for ' + type.name);
    });
  });
};
