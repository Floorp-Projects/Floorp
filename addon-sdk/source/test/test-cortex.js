/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// vim:set ts=2 sw=2 sts=2

"use strict";

var Cortex = require("sdk/deprecated/cortex").Cortex;

exports["test property changes propagate"] = function (assert) {
  var source = {
    _foo: "secret",
    get foo() {
      return this._foo;
    },
    set foo(value) {
      this._foo = value;
    },
    get getOnly() {
      return this._foo;
    },
    set setOnly(value) {
      this._setOnly = value;
    },
    bar: "public",
    method: function method(a, b) {
      return this._foo + a + b
    }
  };
  var fixture = Cortex(source);

  assert.ok(!('_foo' in fixture),
            "properties that start with `_` are omitted");
  assert.equal(fixture.foo, "secret", "get accessor alias works");
  fixture.foo = "new secret";
  assert.equal(fixture.foo, "new secret", "set accessor alias works");
  assert.equal(source.foo, "new secret", "accessor delegates to the source");
  assert.equal(fixture.bar, "public", "data property alias works");
  fixture.bar = "bar";
  assert.equal(source.bar, "bar", "data property change propagates");
  source.bar = "foo"
  assert.equal(fixture.bar, "foo", "data property change probagets back");
  assert.equal(fixture.method("a", "b"), "new secretab",
               "public methods are callable");
  assert.equal(fixture.method.call({ _foo: "test" }, " a,", "b"),
               "new secret a,b",
               "`this` pseudo-variable can not be passed through call.");
  assert.equal(fixture.method.apply({ _foo: "test" }, [" a,", "b"]),
               "new secret a,b",
               "`this` pseudo-variable can not be passed through apply.");
  assert.equal(fixture.getOnly, source._foo,
               "getter returned property of wrapped object");
  fixture.setOnly = 'bar'
  assert.equal(source._setOnly, 'bar', "setter modified wrapped object")
};


exports["test immunity of inheritance"] = function(assert) {
  function Type() {}
  Type.prototype = {
    constructor: Type,
    _bar: 2,
    bar: 3,
    get_Foo: function getFoo() {
      return this._foo;
    }
  }
  var source = Object.create(Type.prototype, {
    _foo: { value: 'secret' },
    getBar: { value: function get_Bar() {
      return this.bar
    }},
    get_Bar: { value: function getBar() {
      return this._bar
    }}
  });

  var fixture = Cortex(source);

  assert.ok(Cortex({}, null, Type.prototype) instanceof Type,
            "if custom prototype is providede cortex will inherit from it");
  assert.ok(fixture instanceof Type,
            "if no prototype is given cortex inherits from object's prototype");

  source.bar += 1;
  assert.notEqual(fixture.bar, source.bar,
                  "chages of properties don't propagate to non-aliases");
  assert.equal(fixture.getBar(), source.bar,
               "methods accessing public properties are bound to the source");

  fixture._bar += 1;
  assert.notEqual(fixture._bar, source._bar,
                  "changes of non aliased properties don't propagate");
  assert.equal(fixture.get_Bar(), source._bar,
               "methods accessing privates are bound to the source");
  assert.notEqual(fixture.get_Foo(), source._foo,
                  "prototoype methods are not bound to the source");
}

exports["test customized public properties"] = function(assert) {
  var source = {
    _a: 'a',
    b: 'b',
    get: function get(name) {
      return this[name];
    }
  };
  
  var fixture = Cortex(source, ['_a', 'get']);
  fixture._a += "#change";


  assert.ok(!("b" in fixture), "non-public own property is not defined");
  assert.equal(fixture.get("b"), source.b,
              "public methods preserve access to the private properties");
  assert.equal(fixture._a, source._a,
              "custom public property changes propagate");
}

//if (require.main == module)
  require("test").run(exports);
