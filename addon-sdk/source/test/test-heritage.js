/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { Class, extend, mix, obscure } = require('sdk/core/heritage');

exports['test extend'] = function(assert) {
  let ancestor = { a: 1 };
  let descendant = extend(ancestor, {
    b: 2,
    get c() { return 3 },
    d: function() { return 4 }
  });

  assert.ok(ancestor.isPrototypeOf(descendant),
            'descendant inherits from ancestor');
  assert.ok(descendant.b, 2, 'proprety was implemented');
  assert.ok(descendant.c, 3, 'getter was implemented');
  assert.ok(descendant.d(), 4, 'method was implemented');

  /* Will be fixed once Bug 674195 is shipped.
  assert.ok(Object.isFrozen(descendant),
            'extend returns frozen objects');
  */
};

exports['test mix'] = function(assert) {
  let ancestor = { a: 1 }
  let mixed = mix(extend(ancestor, { b: 1, c: 1 }), { c: 2 }, { d: 3 });

  assert.deepEqual(JSON.parse(JSON.stringify(mixed)), { b: 1, c: 2, d: 3 },
                   'properties mixed as expected');
  assert.ok(ancestor.isPrototypeOf(mixed),
            'first arguments ancestor is ancestor of result');
};

exports['test obscure'] = function(assert) {
  let fixture = mix({ a: 1 }, obscure({ b: 2 }));

  assert.equal(fixture.a, 1, 'a property is included');
  assert.equal(fixture.b, 2, 'b proprety is included');
  assert.ok(!Object.getOwnPropertyDescriptor(fixture, 'b').enumerable,
            'obscured properties are non-enumerable');
};

exports['test inheritance'] = function(assert) {
  let Ancestor = Class({
    name: 'ancestor',
    method: function () {
      return 'hello ' + this.name;
    }
  });

  assert.ok(Ancestor() instanceof Ancestor,
            'can be instantiated without new');
  assert.ok(new Ancestor() instanceof Ancestor,
            'can also be instantiated with new');
  assert.ok(Ancestor() instanceof Class,
            'if ancestor not specified than defaults to Class');
  assert.ok(Ancestor.prototype.extends, Class.prototype,
            'extends of prototype points to ancestors prototype');


  assert.equal(Ancestor().method(), 'hello ancestor',
               'instance inherits defined properties');

  let Descendant = Class({
    extends: Ancestor,
    name: 'descendant'
  });

  assert.ok(Descendant() instanceof Descendant,
            'instantiates correctly');
  assert.ok(Descendant() instanceof Ancestor,
            'Inherits for passed `extends`');
  assert.equal(Descendant().method(), 'hello descendant',
               'propreties inherited');
};

exports['test immunity against __proto__'] = function(assert) {
  let Foo = Class({ name: 'foo', hacked: false });

  let Bar = Class({ extends: Foo, name: 'bar' });

  assert.throws(function() {
    Foo.prototype.__proto__ = { hacked: true };
    if (Foo() instanceof Base && !Foo().hacked)
      throw Error('can not change prototype chain');
  }, 'prototype chain is immune to __proto__ hacks');

  assert.throws(function() {
    Foo.prototype.__proto__ = { hacked: true };
    if (Bar() instanceof Foo && !Bar().hacked)
      throw Error('can not change prototype chain');
  }, 'prototype chain of decedants immune to __proto__ hacks');
};

exports['test super'] = function(assert) {
  var Foo = Class({
    initialize: function initialize(options) {
      this.name = options.name;
    }
  });

  var Bar = Class({
    extends: Foo,
    initialize: function Bar(options) {
      Foo.prototype.initialize.call(this, options);
      this.type = 'bar';
    }
  });

  var bar = Bar({ name: 'test' });

  assert.equal(bar.type, 'bar', 'bar initializer was called');
  assert.equal(bar.name, 'test', 'bar initializer called Foo initializer');
};

exports['test initialize'] = function(assert) {
  var Dog = Class({
    initialize: function initialize(name) {
      this.name = name;
    },
    type: 'dog',
    bark: function bark() {
      return 'Ruff! Ruff!'
    }
  });

  var fluffy = Dog('Fluffy');   // instatiation
  assert.ok(fluffy instanceof Dog,
            'instanceof works as expected');
  assert.ok(fluffy instanceof Class,
            'inherits form Class if not specified otherwise');
  assert.ok(fluffy.name, 'fluffy',
            'initialize unless specified otherwise');
};

exports['test complements regular inheritace'] = function(assert) {
  let Base = Class({ name: 'base' });

  function Type() {
      // ...
  }
  Type.prototype = Object.create(Base.prototype);
  Type.prototype.run = function() {
    // ...
  };

  let value = new Type();

  assert.ok(value instanceof Type, 'creates instance of Type');
  assert.ok(value instanceof Base, 'inherits from Base');
  assert.equal(value.name, 'base', 'inherits properties from Base');


  let SubType = Class({
    extends: Type,
    sub: 'type'
  });

  let fixture = SubType();

  assert.ok(fixture instanceof Base, 'is instance of Base');
  assert.ok(fixture instanceof Type, 'is instance of Type');
  assert.ok(fixture instanceof SubType, 'is instance of SubType');

  assert.equal(fixture.sub, 'type', 'proprety is defined');
  assert.equal(fixture.run, Type.prototype.run, 'proprety is inherited');
  assert.equal(fixture.name, 'base', 'inherits base properties');
};

exports['test extends object'] = function(assert) {
  let prototype = { constructor: function() { return this; }, name: 'me' };
  let Foo = Class({
    extends: prototype,
    value: 2
  });
  let foo = new Foo();

  assert.ok(foo instanceof Foo, 'instance of Foo');
  assert.ok(!(foo instanceof Class), 'is not instance of Class');
  assert.ok(prototype.isPrototypeOf(foo), 'inherits from given prototype');
  assert.equal(Object.getPrototypeOf(Foo.prototype), prototype,
               'contsructor prototype inherits from extends option');
  assert.equal(foo.value, 2, 'property is defined');
  assert.equal(foo.name, 'me', 'prototype proprety is inherited');
};


var HEX = Class({
  hex: function hex() {
    return '#' + this.color;
  }
});

var RGB = Class({
  red: function red() {
    return parseInt(this.color.substr(0, 2), 16);
  },
  green: function green() {
    return parseInt(this.color.substr(2, 2), 16);
  },
  blue: function blue() {
    return parseInt(this.color.substr(4, 2), 16);
  }
});

var CMYK = Class({
  black: function black() {
    var color = Math.max(Math.max(this.red(), this.green()), this.blue());
    return (1 - color / 255).toFixed(4);
  },
  magenta: function magenta() {
    var K = this.black();
    return (((1 - this.green() / 255).toFixed(4) - K) / (1 - K)).toFixed(4);
  },
  yellow: function yellow() {
    var K = this.black();
    return (((1 - this.blue() / 255).toFixed(4) - K) / (1 - K)).toFixed(4);
  },
  cyan: function cyan() {
    var K = this.black();
    return (((1 - this.red() / 255).toFixed(4) - K) / (1 - K)).toFixed(4);
  }
});

var Color = Class({
  implements: [ HEX, RGB, CMYK ],
  initialize: function initialize(color) {
    this.color = color;
  }
});

exports['test composition'] = function(assert) {
  var pink = Color('FFC0CB');

  assert.equal(pink.red(), 255, 'red() works');
  assert.equal(pink.green(), 192, 'green() works');
  assert.equal(pink.blue(), 203, 'blue() works');

  assert.equal(pink.magenta(), 0.2471, 'magenta() works');
  assert.equal(pink.yellow(), 0.2039, 'yellow() works');
  assert.equal(pink.cyan(), 0.0000, 'cyan() works');

  assert.ok(pink instanceof Color, 'is instance of Color');
  assert.ok(pink instanceof Class, 'is instance of Class');
};

var Point = Class({
  initialize: function initialize(x, y) {
    this.x = x;
    this.y = y;
  },
  toString: function toString() {
    return this.x + ':' + this.y;
  }
})

var Pixel = Class({
  extends: Point,
  implements: [ Color ],
  initialize: function initialize(x, y, color) {
    Color.prototype.initialize.call(this, color);
    Point.prototype.initialize.call(this, x, y);
  },
  toString: function toString() {
    return this.hex() + '@' + Point.prototype.toString.call(this)
  }
});

exports['test compostion with inheritance'] = function(assert) {
  var pixel = Pixel(11, 23, 'CC3399');

  assert.equal(pixel.toString(), '#CC3399@11:23', 'stringifies correctly');
  assert.ok(pixel instanceof Pixel, 'instance of Pixel');
  assert.ok(pixel instanceof Point, 'instance of Point');
};

exports['test composition with objects'] = function(assert) {
  var A = { a: 1, b: 1 };
  var B = Class({ b: 2, c: 2 });
  var C = { c: 3 };
  var D = { d: 4 };

  var ABCD = Class({
    implements: [ A, B, C, D ],
    e: 5
  });

  var f = ABCD();

  assert.equal(f.a, 1, 'inherits A.a');
  assert.equal(f.b, 2, 'inherits B.b overrides A.b');
  assert.equal(f.c, 3, 'inherits C.c overrides B.c');
  assert.equal(f.d, 4, 'inherits D.d');
  assert.equal(f.e, 5, 'implements e');
};

require("test").run(exports);
