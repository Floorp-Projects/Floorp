/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict"

var utils = require("sdk/lang/type");

exports["test function"] = function (assert) {
  assert.equal(utils.isFunction(function(){}), true, "value is a function");
  assert.equal(utils.isFunction(Object), true, "Object is a function");
  assert.equal(utils.isFunction(new Function("")), true, "Genertaed value is a function");
  assert.equal(utils.isFunction({}), false, "object is not a function");
  assert.equal(utils.isFunction(4), false, "number is not a function");
};

exports["test atoms"] = function (assert) {
  assert.equal(utils.isPrimitive(2), true, "number is a primitive");
  assert.equal(utils.isPrimitive(NaN), true, "`NaN` is a primitve");
  assert.equal(utils.isPrimitive(undefined), true, "`undefined` is a primitive");
  assert.equal(utils.isPrimitive(null), true, "`null` is a primitive");
  assert.equal(utils.isPrimitive(Infinity), true, "`Infinity` is a primitive");
  assert.equal(utils.isPrimitive("foo"), true, "strings are a primitive");
  assert.ok(utils.isPrimitive(true) && utils.isPrimitive(false),
            "booleans are primitive");
};

exports["test object"] = function (assert) {
  assert.equal(utils.isObject({}), true, "`{}` is an object");
  assert.ok(!utils.isObject(null), "`null` is not an object");
  assert.ok(!utils.isObject(Object), "functions is not an object");
};

exports["test generator"] = function (assert) {
  assert.equal(utils.isGenerator(function*(){}), true, "`function*(){}` is a generator");
  assert.equal(utils.isGenerator(function(){}), false, "`function(){}` is not a generator");
  assert.equal(utils.isGenerator(() => {}), false, "`() => {}` is not a generator");
  assert.equal(utils.isGenerator({}), false, "`{}` is not a generator");
  assert.equal(utils.isGenerator(1), false, "`1` is not a generator");
  assert.equal(utils.isGenerator([]), false, "`[]` is not a generator");
  assert.equal(utils.isGenerator(null), false, "`null` is not a generator");
  assert.equal(utils.isGenerator(undefined), false, "`undefined` is not a generator");
};

exports["test array"] = function (assert) {
  assert.equal(utils.isArray([]), true, "[] is an array");
  assert.equal(utils.isArray([1]), true, "[1] is an array");
  assert.equal(utils.isArray(new Array()), true, "new Array() is an array");
  assert.equal(utils.isArray(new Array(10)), true, "new Array(10) is an array");
  assert.equal(utils.isArray(Array.prototype), true, "Array.prototype is an array");

  assert.equal(utils.isArray(), false, "implicit undefined is not an array");
  assert.equal(utils.isArray(null), false, "null is not an array");
  assert.equal(utils.isArray(undefined), false, "undefined is not an array");
  assert.equal(utils.isArray(1), false, "1 is not an array");
  assert.equal(utils.isArray(true), false, "true is not an array");
  assert.equal(utils.isArray('foo'), false, "'foo' is not an array");
  assert.equal(utils.isArray({}), false, "{} is not an array");
  assert.equal(utils.isArray(Symbol.iterator), false, "Symbol.iterator is not an array");
};

exports["test arguments"] = function (assert) {
  assert.equal(utils.isArguments(arguments), true, "arguments is an arguments");
  (function() {
    assert.equal(utils.isArguments(arguments), true, "arguments in nested function is an arguments");
  })();
  (function*() {
    assert.equal(utils.isArguments(arguments), true, "arguments in nested generator is an arguments");
  })();
  (() => {
    assert.equal(utils.isArguments(arguments), true, "arguments in arrow function is an arguments");
  })();

  assert.equal(utils.isArguments(), false, "implicit undefined is not an arguments");
  assert.equal(utils.isArguments(null), false, "null is not an arguments");
  assert.equal(utils.isArguments(undefined), false, "undefined is not an arguments");
  assert.equal(utils.isArguments(1), false, "1 is not an arguments");
  assert.equal(utils.isArguments(true), false, "true is not an arguments");
  assert.equal(utils.isArguments('foo'), false, "'foo' is not an arguments");
  assert.equal(utils.isArguments([]), false, "[] is not an arguments");
  assert.equal(utils.isArguments({}), false, "{} is not an arguments");
  assert.equal(utils.isArguments(Symbol.iterator), false, "Symbol.iterator is not an arguments");
  (function(...args) {
    assert.equal(utils.isArguments(args), false, "rest arguments is not an arguments");
  })();
};

exports["test flat objects"] = function (assert) {
  assert.ok(utils.isFlat({}), "`{}` is a flat object");
  assert.ok(!utils.isFlat([]), "`[]` is not a flat object");
  assert.ok(!utils.isFlat(new function() {}), "derived objects are not flat");
  assert.ok(utils.isFlat(Object.prototype), "Object.prototype is flat");
};

exports["test json atoms"] = function (assert) {
  assert.ok(utils.isJSON(null), "`null` is JSON");
  assert.ok(utils.isJSON(undefined), "`undefined` is JSON");
  assert.ok(utils.isJSON(NaN), "`NaN` is JSON");
  assert.ok(utils.isJSON(Infinity), "`Infinity` is JSON");
  assert.ok(utils.isJSON(true) && utils.isJSON(false), "booleans are JSON");
  assert.ok(utils.isJSON(4), utils.isJSON(0), "numbers are JSON");
  assert.ok(utils.isJSON("foo bar"), "strings are JSON");
};

exports["test instanceOf"] = function (assert) {
  assert.ok(utils.instanceOf(assert, Object),
            "assert is object from other sandbox");
  assert.ok(utils.instanceOf(new Date(), Date), "instance of date");
  assert.ok(!utils.instanceOf(null, Object), "null is not an instance");
};

exports["test json"] = function (assert) {
  assert.ok(!utils.isJSON(function(){}), "functions are not json");
  assert.ok(utils.isJSON({}), "`{}` is JSON");
  assert.ok(utils.isJSON({
              a: "foo",
              b: 3,
              c: undefined,
              d: null,
              e: {
                f: {
                  g: "bar",
                  p: [{}, "oueou", 56]
                },
                q: { nan: NaN, infinity: Infinity },
                "non standard name": "still works"
              }
            }), "JSON can contain nested objects");

  var foo = {};
  var bar = { foo: foo };
  foo.bar = bar;
  assert.ok(!utils.isJSON(foo), "recursive objects are not json");


  assert.ok(!utils.isJSON({ get foo() { return 5 } }),
            "json can not have getter");

  assert.ok(!utils.isJSON({ foo: "bar", baz: function () {} }),
            "json can not contain functions");

  assert.ok(!utils.isJSON(Object.create({})),
            "json must be direct descendant of `Object.prototype`");
};

require("sdk/test").run(exports);
