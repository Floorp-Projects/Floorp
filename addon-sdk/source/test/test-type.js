/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict"

var utils = require("sdk/lang/type");

exports["test function"] = function (assert) {
  assert.ok(utils.isFunction(function(){}), "value is function");
  assert.ok(utils.isFunction(Object), "Object is function");
  assert.ok(utils.isFunction(new Function("")), "Genertaed value is function");
  assert.ok(!utils.isFunction({}), "object is not a function");
  assert.ok(!utils.isFunction(4), "number is not a function");
};

exports["test atoms"] = function (assert) {
  assert.ok(utils.isPrimitive(2), "number is primitive");
  assert.ok(utils.isPrimitive(NaN), "`NaN` is primitve");
  assert.ok(utils.isPrimitive(undefined), "`undefined` is primitive");
  assert.ok(utils.isPrimitive(null), "`null` is primitive");
  assert.ok(utils.isPrimitive(Infinity), "`Infinity` is primitive");
  assert.ok(utils.isPrimitive("foo"), "strings are primitive");
  assert.ok(utils.isPrimitive(true) && utils.isPrimitive(false),
            "booleans are primitive");
};

exports["test object"] = function (assert) {
  assert.ok(utils.isObject({}), "`{}` is object");
  assert.ok(!utils.isObject(null), "`null` is not an object");
  assert.ok(!utils.isObject(Object), "functions is not an object");
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

require("test").run(exports);
