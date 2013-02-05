/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

var Trait = require("sdk/deprecated/light-traits").Trait;
var utils = require("./utils");
var Data = utils.Data;
var Method = utils.Method;
var Accessor = utils.Accessor;
var Required = utils.Required;
var Conflict = utils.Conflict;

function method() {}

exports.Assert = require("./assert").Assert
exports["test simple composition"] = function(assert) {
  var actual = Trait.compose(
    Trait({ a: 0, b: 1 }),
    { c: { value: 2 }, d: { value: method, enumerable: true } }
  );

  var expected = {
    a: Data(0),
    b: Data(1),
    c: Data(2, false, false, false),
    d: Method(method, true, false, false)
  };

  assert.equalTraits(actual, expected);
};

exports["test composition with conflict"] = function(assert) {
  var actual = Trait.compose(
    Trait({ a: 0, b: 1 }),
    {
      a: {
        value: 2,
        writable: true,
        configurable: true,
        enumerable: true
      },
      c: {
        value: method,
        configurable: true
      }
    }
  );

  var expected = {
    a: Conflict("a"),
    b: Data(1),
    c: Method(method, false, true, false)
  };

  assert.equalTraits(actual, expected);
};

exports["test identical props does not cause conflict"] = function(assert) {
  var actual = Trait.compose(
    {
      a: {
        value: 0,
        writable: true,
        configurable: true,
        enumerable: true
      },
      b: {
        value: 1
      }
    },
    Trait({
      a: 0,
      c: method
    })
  );

  var expected = {
    a: Data(0),
    b: Data(1, false, false, false),
    c: Method(method)
  }

  assert.equalTraits(actual, expected);
};

exports["test composition with identical required props"] = function(assert) {
  var actual = Trait.compose(
    Trait({ a: Trait.required, b: 1 }),
    { a: { required: true }, c: { value: method } }
  );

  var expected = {
    a: Required(),
    b: Data(1),
    c: Method(method, false, false, false)
  };

  assert.equalTraits(actual, expected);
};

exports["test composition satisfying a required prop"] = function(assert) {
  var actual = Trait.compose(
    Trait({ a: Trait.required, b: 1 }),
    { a: { value: method, enumerable: true } }
  );

  var expected = {
    a: Method(method, true, false, false),
    b: Data(1)
  };

  assert.equalTraits(actual, expected);
};

exports["test compose is neutral wrt conflicts"] = function(assert) {
  var actual = Trait.compose(
    Trait({ a: { value: 1 } }, Trait({ a: 2 })),
    { b: { value: 0, writable: true, configurable: true, enumerable: false } }
  );

  var expected = { a: Conflict("a"), b: Data(0, false) };

  assert.equalTraits(actual, expected);
};

exports["test conflicting prop overrides Trait.required"] = function(assert) {
  var actual = Trait.compose(
    Trait.compose(
      Trait({ a: 1 }),
      { a: { value: 2 } }
    ),
    { a: { value: Trait.required } }
  );

  var expected = { a: Conflict("a") };

  assert.equalTraits(actual, expected);
};

exports["test compose is commutative"] = function(assert) {
  var actual = Trait.compose(
    Trait({ a: 0, b: 1 }),
    { c: { value: 2 }, d: { value: method } }
  );

  var expected = Trait.compose(
    { c: { value: 2 }, d: { value: method } },
    Trait({ a: 0, b: 1 })
  );

  assert.equalTraits(actual, expected);
}

exports["test compose is commutative, also for required/conflicting props"] = function(assert) {
  var actual = Trait.compose(
    {
      a: { value: 0 },
      b: { value: 1 },
      c: { value: 3 },
      e: { value: Trait.required }
    },
    {
      c: { value: 2 },
      d: { get: method }
    }
  );

  var expected = Trait.compose(
    Trait({ c: 3 }),
    {
      c: { value: 2 },
      d: { get: method },
      a: { value: 0 },
      b: { value: 1 },
      e: { value: Trait.required },
    }
  );

  assert.equalTraits(actual, expected);
};

exports["test compose is associative"] = function(assert) {
  var actual = Trait.compose(
    {
      a: { value: 0 },
      b: { value: 1 },
      c: { value: 3 },
      d: { value: Trait.required }
    },
    Trait.compose(
      { c: { value: 3 }, d: { value: Trait.required } },
      { c: { value: 2 }, d: { value: method }, e: { value: "foo" } }
    )
  );

  var expected = Trait.compose(
    Trait.compose(
      {
        a: { value: 0 },
        b: { value: 1 },
        c: { value: 3 },
        d: { value: Trait.required }
      },
      {
        c: { value: 3 },
        d: { value: Trait.required }
      }
    ),
    {
      c: { value: 2 },
      d: { value: method },
      e: { value: "foo" }
    }
  );

  assert.equalTraits(actual, expected);
};

exports["test diamond import of same prop do not conflict"] = function(assert) {
  var actual = Trait.compose(
    Trait.compose(
      { b: { value: 2 } },
      { a: { value: 1, enumerable: true, configurable: true, writable: true } }
    ),
    Trait.compose(
      { c: { value: 3 } },
      Trait({ a: 1 })
    ),
    Trait({ d: 4 })
  );

  var expected = {
    a: Data(1),
    b: Data(2, false, false, false),
    c: Data(3, false, false, false),
    d: Data(4)
  };

  assert.equalTraits(actual, expected);
};

exports["test create simple"] = function(assert) {
  var o1 = Trait.compose(
    Trait({ a: 1 }),
    {
      b: {
        value: function() {
          return this.a;
        }
      }
    }
  ).create(Object.prototype);

  assert.equal(Object.getPrototypeOf(o1), Object.prototype, "o1 prototype");
  assert.equal(1, o1.a, "o1.a");
  assert.equal(1, o1.b(), "o1.b()");
  assert.equal(Object.keys(o1).length, 1, "Object.keys(o1).length === 2");
};

exports["test create with Array.prototype"] = function(assert) {
  var o2 = Trait.compose({}, {}).create(Array.prototype);
  assert.equal(Object.getPrototypeOf(o2), Array.prototype, "o2 prototype");
};

exports["test exception for incomplete required properties"] = function(assert) {
  assert.throws(function() {
    Trait({ foo: Trait.required }).create(Object.prototype)
  }, /Missing required property: `foo`/, "required prop error");
}

exports["test exception for unresolved conflicts"] = function(assert) {
  assert.throws(function() {
    Trait(Trait({ a: 0 }), Trait({ a: 1 })).create({})
  }, /Remaining conflicting property: `a`/, "conflicting prop error");
}

exports["test conflicting properties are present"] = function(assert) {
  var o5 = Object.create(Object.prototype, Trait.compose(
    { a: { value: 0 } },
    { a: { value: 1 } }
  ));

  assert.ok("a" in o5, "conflicting property present");
  assert.throws(function() {
    o5.a
  }, /Remaining conflicting property: `a`/, "conflicting prop access error");
};

exports["test diamond with conflicts"] = function(assert) {
  function makeT1(x) {
    return {
      m: {
        value: function() {
          return x
        }
      }
    };
  };

  function makeT2(x) {
    return Trait.compose(
      Trait({ t2: "foo" }),
      makeT1(x)
    );
  };

  function makeT3(x) {
    return Trait.compose(
      {
        t3: { value: "bar" }
      },
      makeT1(x)
    );
  };

  var T4 = Trait.compose(makeT2(5), makeT3(5));

  assert.throws(function() {
    T4.create(Object.prototype);
  }, /Remaining conflicting property: `m`/, "diamond prop conflict");
};

exports["test providing requirements through proto"] = function(assert) {
  var t = Trait.compose(
    {},
    { required: { required: true } }
  ).create({ required: "test" });

  assert.equal(t.required, "test", "property from proto is inherited");
};

if (module == require.main)
  require("test").run(exports);
