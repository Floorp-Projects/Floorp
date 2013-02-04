<!-- This Source Code Form is subject to the terms of the Mozilla Public
   - License, v. 2.0. If a copy of the MPL was not distributed with this
   - file, You can obtain one at http://mozilla.org/MPL/2.0/. -->

Doing [inheritance in JavaScript](https://developer.mozilla.org/en/Introduction_to_Object-Oriented_JavaScript)
is both verbose and painful. Reading or writing such code requires sharp eye
and lot's of discipline, mainly due to code fragmentation and lots of machinery
being exposed:

    // Defining a simple Class
    function Dog(name) {
      // Classes are for creating instances, calling them without `new` changes
      // behavior, which in majority cases you need to handle, so you end up
      // with additional boilerplate.
      if (!(this instanceof Dog)) return new Dog(name);

      this.name = name;
    };
    // To define methods you need to make a dance with a special 'prototype'
    // property of the constructor function. This is too much machinery exposed.
    Dog.prototype.type = 'dog';
    Dog.prototype.bark = function bark() {
      return 'Ruff! Ruff!'
    };

    // Subclassing a `Dog`
    function Pet(name, breed) {
      // Once again we do our little dance 
      if (!(this instanceof Pet)) return new Pet(name, breed);

      Dog.call(this, name);
      this.breed = breed;
    }
    // To subclass, you need to make another special dance with special
    // 'prototype' properties.
    Pet.prototype = Object.create(Dog.prototype);
    // If you want correct instanceof behavior you need to make a dance with
    // another special `constructor` property of the `prototype` object.
    Object.defineProperty(Pet.prototype, 'contsructor', { value: Pet });
    // Finally you can define some properties.
    Pet.prototype.call = function(name) {
      return this.name === name ? this.bark() : '';
    };

Since SDK APIs may be interacting with untrusted code an extra security
measures are required to guarantee that documented behavior can't be changed
at runtime. To do that we need to freeze constructor's prototype chain to
make sure functions are frozen:

    Object.freeze(Dog.prototype);
    Object.freeze(Pet.prototype);

*Note: Also, this is not quite enough as `Object.prototype` stays mutable &
in fact we do little bit more in SDK to address that, but it's not in the scope
of this documentation.*

Doing all of this manually is both tedious and error prone task. That is why
SDK provides utility functions to make it more declarative and less verbose.

## Class

Module exports `Class` utility function for making `constructor` functions
with a proper `prototype` chain setup in declarative manner:

    var { Class } = require('sdk/core/heritage');
    var Dog = Class({
      initialize: function initialize(name) {
        this.name = name;
      },
      type: 'dog',
      bark: function bark() {
        return 'Ruff! Ruff!'
      }
    });

Note: We use term `Class` to refer an exemplar constructs in a form of
constructor functions with a proper prototype chain setup. Constructors
created using `Class` function don't require `new` keyword (even though
it can be used) for instantiation. Also, idiomatic SDK code does not uses
optional `new` keywords, but you're free to use it in your add-on code:

    var fluffy = Dog('Fluffy');   // instatiation
    fluffy instanceof Dog         // => true
    fluffy instanceof Class       // => true

As you could notice from example above classes created via `Class` function
by default inherits from a `Class` itself. Also you could specify which class
you want to inherit from by passing special `extends` property:

    var Pet = Class({
      extends: Dog,              // should inherit from Dog
      initialize: function initialize(breed, name) {
        // To call ancestor methods you will have to access them
        // explicitly
        Dog.prototype.initialize.call(this, name);
        this.breed = breed;
      },
      call: function call(name) {
        return this.name === name ? this.bark() : '';
      }
    });

    var tsuga = Pet('Labrador', 'Tsuga');
    tsuga instanceof Pet                    // => true
    tsuga instanceof Dog                    // => true
    tsuga.call('Tsuga')                     // => 'Ruff! Ruff!'

Please note that `Class` is just an utility function which we use in SDK, and
recommend our users to use it, but it's in no way enforced. As a matter of fact
since result is just a plain constructor function with proper prototype chain
setup you could sub-class it as any other constructor:

    function Labrador() {
      // ...
    }
    Labrador.prototype = Object.create(Dog.prototype);
    Labrador.prototype.jump = function() {
      // ...
    }

    var leo = new Labrador()
    leo.bark();                           // => 'Ruff! Ruff!'
    leo.instanceof Labrador               // => true
    leo.instanceof Dog                    // => true

Also, you could use `Class` function to subclass constructor functions that
were not created by a `Class` itself:

    var Foo = Class({
      extends: Labrador
      // ...
    })

Sometimes (single) inheritance is not enough and defining reusable, composable
pieces of functionality does a better job:

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

Such composable pieces can be combined into a single class definition by
passing special `implements` option to a `Class` function:


    // Composing `Color` prototype out of reusable components:
    var Color = Class({
      implements: [ HEX, RGB, CMYK ],
      initialize: function initialize(color) {
        this.color = color;
      }
    });

    var pink = Color('FFC0CB');

    // RGB
    pink.red()                  // => 255
    pink.green()                // => 192
    pink.blue()                 // => 203

    // CMYK
    pink.magenta()              // => 0.2471
    pink.yellow()               // => 0.2039
    pink.cyan()                 // => 0.0000

    pink instanceof Color       // => true

Be aware though that it's not multiple inheritance and ancestors prototypes of
the classes passed under `implements` option are ignored. As mentioned before
you could pass constructors that were not created using `Class` function as
long as they have proper `prototype` setup.

Also you can mix inheritance and composition together if necessary:

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

    var pixel = Pixel(11, 23, 'CC3399');
    pixel.toString();                     // => #CC3399@11:23
    pixel instanceof Pixel                // => true
    pixel instanceof Point                // => true

## extend

Module exports `extend` utility function, that is useful for creating objects
that inherit from other objects, without associated classes. It's very similar
to `Object.create`, only difference is that second argument is an object
containing properties to be defined, instead of property descriptor map. Also,
keep in mind that returned object will be frozen.

    var { extend } = require('sdk/core/heritage');
    var base = { a: 1 };
    var derived = extend(base, { b: 2 });

    derived.a                         // => 1
    derived.b                         // => 2
    base.isPrototypeOf(derived)       // => true

## mix

Module exports `mix` utility function that is useful for mixing properties of
multiple objects into a single one. Function takes arbitrary number of source
objects and returns a fresh object that inherits from the same prototype as a
first one and implements all own properties of all given objects. If two or
more argument objects have own properties with the same name, the property is
overridden, with precedence from right to left, implying, that properties of
the object on the left are overridden by a same named property of the object
on the right.


    var { mix } = require('sdk/core/heritage');
    var object = mix({ a: 1, b: 1 }, { b: 2 }, { c: 3 });
    JSON.stringify(object)            // => { "a": 1, "b": 2, "c": 3 }


## obscure

Module exports `obscure` utility function that is useful for defining
`non-enumerable` properties. Function takes an object and returns a new one
in return that inherits from the same object as given one and implements all
own properties of given object but as non-enumerable ones:

    var { obscure } = require('api-utils/heritage');
    var object = mix({ a: 1 }, obscure({ b: 2 }));
    Object.getOwnPropertyNames(foo);    // => [ 'a' ]
