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

const TEST_URI = "data:text/html;charset=utf-8,<p id='gcli-input'>gcli-testTypes.js</p>";

function test() {
  helpers.addTabWithToolbar(TEST_URI, function(options) {
    return helpers.runTests(options, exports);
  }).then(finish);
}

// <INJECTED SOURCE:END>

'use strict';

// var assert = require('test/assert');
var types = require('gcli/types');

function forEachType(options, typeSpec, callback) {
  types.getTypeNames().forEach(function(name) {
    typeSpec.name = name;
    typeSpec.requisition = options.display.requisition;

    // Provide some basic defaults to help selection/delegate/array work
    if (name === 'selection') {
      typeSpec.data = [ 'a', 'b' ];
    }
    else if (name === 'delegate') {
      typeSpec.delegateType = function() {
        return types.getType('string');
      };
    }
    else if (name === 'array') {
      typeSpec.subtype = 'string';
    }

    var type = types.getType(typeSpec);
    callback(type);
  });
}

exports.testDefault = function(options) {
  if (options.isJsdom) {
    assert.log('Skipping tests due to issues with resource type.');
    return;
  }

  forEachType(options, {}, function(type) {
    var blank = type.getBlank().value;

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
  forEachType(options, { defaultValue: null }, function(type) {
    assert.is(type.stringify(null, null), '', 'stringify(null) for ' + type.name);
  });
};


// });
