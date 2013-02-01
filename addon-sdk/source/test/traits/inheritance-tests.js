/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

var Trait = require("sdk/deprecated/light-traits").Trait;

exports["test custom constructor and inherited toString"] = function(assert) {
  function Type() {
    return Object.create(Type.prototype);
  }
  Type.prototype = Trait({
    method: function method() {
      return 2;
    }
  }).create(Object.freeze(Type.prototype));

  var fixture = Type();

  assert.equal(fixture.constructor, Type, "must override constructor");
  assert.equal(fixture.toString(), "[object Type]", "must inherit toString");
};

exports["test custom toString and inherited constructor"] = function(assert) {
  function Type() {
    return Object.create(Type.prototype);
  }
  Type.prototype = Trait({
    toString: function toString() {
      return "<toString>";
    }
  }).create();

  var fixture = Type();

  assert.equal(fixture.constructor, Trait, "must inherit constructor Trait");
  assert.equal(fixture.toString(), "<toString>", "Must override toString");
};

exports["test custom toString and constructor"] = function(assert) {
  function Type() {
    return TypeTrait.create(Type.prototype);
  }
  Object.freeze(Type.prototype);
  var TypeTrait = Trait({
    toString: function toString() {
      return "<toString>";
    }
  });

  var fixture = Type();

  assert.equal(fixture.constructor, Type, "constructor is provided to create");
  assert.equal(fixture.toString(), "<toString>", "toString was overridden");
};

exports["test resolve constructor"] = function (assert) {
  function Type() {}
  var T1 = Trait({ constructor: Type }).resolve({ constructor: '_foo' });
  var f1 = T1.create();

  assert.equal(f1._foo, Type, "constructor was resolved");
  assert.equal(f1.constructor, Trait, "constructor of prototype is inherited");
  assert.equal(f1.toString(), "[object Trait]", "toString is inherited");
};

exports["test compose read-only"] = function (assert) {
  function Type() {}
  Type.prototype = Trait.compose(Trait({}), {
    constructor: { value: Type },
    a: { value: "b", enumerable: true }
  }).resolve({ a: "b" }).create({ a: "a" });

  var f1 = new Type();

  assert.equal(Object.getPrototypeOf(f1), Type.prototype, "inherits correctly");
  assert.equal(f1.constructor, Type, "constructor was overridden");
  assert.equal(f1.toString(), "[object Type]", "toString was inherited");
  assert.equal(f1.a, "a", "property a was resolved");
  assert.equal(f1.b, "b", "property a was renamed to b");
  assert.ok(!Object.getOwnPropertyDescriptor(Type.prototype, "a"),
            "a is not on the prototype of the instance");

  var proto = Object.getPrototypeOf(Type.prototype);
  var dc = Object.getOwnPropertyDescriptor(Type.prototype, "constructor");
  var db = Object.getOwnPropertyDescriptor(Type.prototype, "b");
  var da = Object.getOwnPropertyDescriptor(proto, "a");

  assert.ok(!dc.writable, "constructor is not writable");
  assert.ok(!dc.enumerable, "constructor is not enumerable");
  assert.ok(dc.configurable, "constructor inherits configurability");

  assert.ok(!db.writable, "a -> b is not writable");
  assert.ok(db.enumerable, "a -> b is enumerable");
  assert.ok(!db.configurable, "a -> b is not configurable");

  assert.ok(da.writable, "a is writable");
  assert.ok(da.enumerable, "a is enumerable");
  assert.ok(da.configurable, "a is configurable");
};

if (require.main == module)
  require("test").run(exports);
