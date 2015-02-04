/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

const { ns } = require("sdk/core/namespace");
const { Cc, Ci, Cu } = require("chrome");
const { setTimeout } = require("sdk/timers")

exports["test post GC references"] = function (assert, done) {
  var target = {}, local = ns()
  local(target).there = true

  assert.equal(local(target).there, true, "namespaced preserved");

  Cu.schedulePreciseGC(function() {
    assert.equal(local(target).there, true, "namespace is preserved post GC");
    done();
  });
};

exports["test namsepace basics"] = function(assert) {
  var privates = ns();
  var object = { foo: function foo() { return "hello foo"; } };

  assert.notEqual(privates(object), object,
                  "namespaced object is not the same");
  assert.ok(!('foo' in privates(object)),
            "public properties are not in the namespace");

  assert.equal(privates(object), privates(object),
               "same namespaced object is returned on each call");
};

exports["test namespace overlays"] = function(assert) {
  var _ = ns();
  var object = { foo: 'foo' };

  _(object).foo = 'bar';

  assert.equal(_(object).foo, "bar",
               "namespaced property `foo` changed value");

  assert.equal(object.foo, "foo",
               "public property `foo` has original value");

  object.foo = "baz";
  assert.equal(_(object).foo, "bar",
               "property changes do not affect namespaced properties");

  object.bar = "foo";
  assert.ok(!("bar" in _(object)),
              "new public properties are not reflected in namespace");
};

exports["test shared namespaces"] = function(assert) {
  var _ = ns();

  var f1 = { hello: 1 };
  var f2 = { foo: 'foo', hello: 2 };
  _(f1).foo = _(f2).foo = 'bar';

  assert.equal(_(f1).hello, _(f2).hello, "namespace can be shared");
  assert.notEqual(f1.hello, _(f1).hello, "shared namespace can overlay");
  assert.notEqual(f2.hello, _(f2).hello, "target is not affected");

  _(f1).hello = 3;

  assert.notEqual(_(f1).hello, _(f2).hello,
                  "namespaced property can be overided");
  assert.equal(_(f2).hello, _({}).hello, "namespace does not change");
};

exports["test multi namespace"] = function(assert) {
  var n1 = ns();
  var n2 = ns();
  var object = { baz: 1 };
  n1(object).foo = 1;
  n2(object).foo = 2;
  n1(object).bar = n2(object).bar = 3;

  assert.notEqual(n1(object).foo, n2(object).foo,
                  "object can have multiple namespaces");
  assert.equal(n1(object).bar, n2(object).bar,
               "object can have matching props in diff namespaces");
};

exports["test ns alias"] = function(assert) {
  assert.strictEqual(ns, require('sdk/core/namespace').Namespace,
                      "ns is an alias of Namespace");
};

exports["test ns inheritance"] = function(assert) {
  let _ = ns();

  let prototype = { level: 1 };
  let object = Object.create(prototype);
  let delegee = Object.create(object);

  _(prototype).foo = {};

  assert.ok(!Object.prototype.hasOwnProperty.call(_(delegee), "foo"),
            "namespaced property is not copied to descendants");
  assert.equal(_(delegee).foo, _(prototype).foo,
               "namespaced properties are inherited by descendants");

  _(object).foo = {};
  assert.notEqual(_(object).foo, _(prototype).foo,
                  "namespaced properties may be shadowed");
  assert.equal(_(object).foo, _(delegee).foo,
               "shadwed properties are inherited by descendants");

  _(object).bar = {};
  assert.ok(!("bar" in _(prototype)),
            "descendants properties are not copied to ancestors");
  assert.ok(_(object).bar, _(delegee).bar,
            "descendants properties are inherited");
};

require("sdk/test").run(exports);
