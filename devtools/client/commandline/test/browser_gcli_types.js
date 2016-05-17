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

"use strict";

// THIS FILE IS GENERATED FROM SOURCE IN THE GCLI PROJECT
// PLEASE TALK TO SOMEONE IN DEVELOPER TOOLS BEFORE EDITING IT

const exports = {};

function test() {
  helpers.runTestModule(exports, "browser_gcli_types.js");
}

// var assert = require('../testharness/assert');
var util = require("gcli/util/util");

function forEachType(options, templateTypeSpec, callback) {
  var types = options.requisition.system.types;
  return util.promiseEach(types.getTypeNames(), function (name) {
    var typeSpec = {};
    util.copyProperties(templateTypeSpec, typeSpec);
    typeSpec.name = name;
    typeSpec.requisition = options.requisition;

    // Provide some basic defaults to help selection/delegate/array work
    if (name === "selection") {
      typeSpec.data = [ "a", "b" ];
    }
    else if (name === "delegate") {
      typeSpec.delegateType = function () {
        return "string";
      };
    }
    else if (name === "array") {
      typeSpec.subtype = "string";
    }
    else if (name === "remote") {
      return;
    }
    else if (name === "union") {
      typeSpec.alternatives = [{ name: "string" }];
    }
    else if (options.isRemote) {
      if (name === "node" || name === "nodelist") {
        return;
      }
    }

    var type = types.createType(typeSpec);
    var reply = callback(type);
    return Promise.resolve(reply);
  });
}

exports.testDefault = function (options) {
  return forEachType(options, {}, function (type) {
    var context = options.requisition.executionContext;
    var blank = type.getBlank(context).value;

    // boolean and array types are exempt from needing undefined blank values
    if (type.name === "boolean") {
      assert.is(blank, false, "blank boolean is false");
    }
    else if (type.name === "array") {
      assert.ok(Array.isArray(blank), "blank array is array");
      assert.is(blank.length, 0, "blank array is empty");
    }
    else if (type.name === "nodelist") {
      assert.ok(typeof blank.item, "function", "blank.item is function");
      assert.is(blank.length, 0, "blank nodelist is empty");
    }
    else {
      assert.is(blank, undefined, "default defined for " + type.name);
    }
  });
};

exports.testNullDefault = function (options) {
  var context = null; // Is this test still valid with a null context?

  return forEachType(options, { defaultValue: null }, function (type) {
    var reply = type.stringify(null, context);
    return Promise.resolve(reply).then(function (str) {
      assert.is(str, "", "stringify(null) for " + type.name);
    });
  });
};

exports.testGetSpec = function (options) {
  return forEachType(options, {}, function (type) {
    if (type.name === "param") {
      return;
    }

    var spec = type.getSpec("cmd", "param");
    assert.ok(spec != null, "non null spec for " + type.name);

    var str = JSON.stringify(spec);
    assert.ok(str != null, "serializable spec for " + type.name);

    var example = options.requisition.system.types.createType(spec);
    assert.ok(example != null, "creatable spec for " + type.name);
  });
};
