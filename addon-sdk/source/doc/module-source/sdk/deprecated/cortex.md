<!-- This Source Code Form is subject to the terms of the Mozilla Public
   - License, v. 2.0. If a copy of the MPL was not distributed with this
   - file, You can obtain one at http://mozilla.org/MPL/2.0/. -->

<div class="warning">
<p>The <code>cortex</code> module is deprecated.</p>
<p>To provide property encapsulation for modules you implement, use the
<a href="modules/sdk/core/namespace.html"><code>namespace</code></a> module.</p>
</div>

## Property Encapsulation ##

In JavaScript it is not possible to create properties that have limited or
controlled accessibility. It is possible to create non-enumerable and
non-writable properties, but still they can be discovered and accessed.
Usually so called "closure capturing" is used to encapsulate such properties
in lexical scope:

    function Foo() {
      var _secret = 'secret';
      this.hello = function hello() {
        return 'Hello ' + _secret;
      }
    }

This provides desired result, but has side effect of degrading code readability,
especially with object-oriented programs. Another disadvantage with this pattern
is that there is no immediate solution for inheriting access to the privates
(illustrated by the following example):

    function Derived() {
      this.hello = function hello() {
        return _secret;
      }
      this.bye = function bye() {
        return _secret;
      }
    }
    Derived.prototype = Object.create(Foo.prototype);

## Facade Objects ##

Alternatively constructor can returned facade objects - proxies to the
instance's public properties:

    function Foo() {
      var foo = Object.create(Foo.prototype);
      return {
        bar: foo.hello.bind(foo);
      }
    }
    Foo.prototype._secret = 'secret';
    Foo.prototype.hello = function hello() {
      return 'Hello ' + this._secret;
    }

    function Derived() {
      var derived = Object.create(Derived.prototype);
      return {
        bar: derived.hello.bind(derived);
        bye: derived.bye.bind(derived);
      }
    }
    Derived.prototype = Object.create(Foo.prototype);
    Derived.prototype.bye = function bye() {
      return 'Bye ' + this._secret;
    };

While this solution solves given issue and provides proper encapsulation for
both own and inherited private properties, it does not addresses following:

 - Privates defined on the `prototype` can be compromised, since they are
   accessible through the constructor (`Foo.prototype._secret`).
 - Behavior of `instanceof` is broken, since `new Derived() instanceof Derived`
   is going to evaluate to `false`.

## Tamper Proofing with Property Descriptor Maps ##

In ES5 new property descriptor maps were introduced, which can be used as a
building blocks for defining reusable peace of functionality. To some degree
they are similar to a `prototype` objects, and can be used so to define pieces
of functionality that is considered to be private (In contrast to `prototype`
they are not exposed by default).

    function Foo() {
      var foo = Object.create(Foo.prototype, FooDescriptor);
      var facade = Object.create(Foo.prototype);
      facade.hello = foo.hello.bind(foo);
      return facade;
    }
    Foo.prototype.hello = function hello() {
      return 'Hello ' + this._secret;
    }
    var FooDescriptor = {
      _secret: { value: 'secret' };
    }

    function Derived() {
      var derived = Object.create(Derived.prototype, DerivedDescriptor);
      var facade = Object.create(Derived.prototype);
      facade.hello = derived.hello.bind(derived);
      facade.bye = derived.bye.bind(derived);
      return facade;
    }
    Derived.prototype = Object.create(Foo.prototype);
    Derived.prototype.bye = function bye() {
      return 'Bye ' + this._secret;
    };
    DerivedDescriptor = {};

    Object.keys(FooDescriptor).forEach(function(key) {
      DerivedDescriptor[key] = FooDescriptor[key];
    });

## Cortex Objects ##

Last approach solves all of the concerns, but adds complexity, verbosity
and decreases code readability. Combination of `Cortex`'s and `Trait`'s
will gracefully solve all these issues and keep code clean:

    var Cortex = require('cortex').Cortex;
    var Trait = require('light-traits').Trait;

    var FooTrait = Trait({
      _secret: 'secret',
      hello: function hello() {
        return 'Hello ' + this._secret;
      }
    });
    function Foo() {
      return Cortex(FooTrait.create(Foo.prototype));
    }

    var DerivedTrait = Trait.compose(FooTrait, Trait({
      bye: function bye() {
        return 'Bye ' + this._secret;
      }
    }));
    function Derived() {
      var derived = DerivedTrait.create(Derived.prototype);
      return Cortex(derived);
    }

Function `Cortex` takes any object and returns a proxy for its public
properties. By default properties are considered to be public if they don't
start with `"_"`, but default behavior can be overridden if needed, by passing
array of public property names as a second argument.

## Gotchas ##

`Cortex` is just a utility function to create a proxy object, and it does not
solve the `prototype`-related issues highlighted earlier, but since traits make
use of property descriptor maps instead of `prototype`s, there aren't any
issues with using `Cortex` to wrap objects created from traits.

If you want to use `Cortex` with an object that uses a `prototype` chain,
however, you should either make sure you don't have any private properties
in the prototype chain or pass the optional third `prototype` argument.

In the latter case, the returned proxy will inherit from the given prototype,
and the `prototype` chain of the wrapped object will be inaccessible.
However, note that the behavior of the `instanceof` operator will vary,
as `proxy instanceof Constructor` will return false even if the Constructor
function's prototype is in the wrapped object's prototype chain.

