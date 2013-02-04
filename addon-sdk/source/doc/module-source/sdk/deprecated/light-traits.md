<!-- This Source Code Form is subject to the terms of the Mozilla Public
   - License, v. 2.0. If a copy of the MPL was not distributed with this
   - file, You can obtain one at http://mozilla.org/MPL/2.0/. -->

<div class="warning">
<p>The <code>light-traits</code> module is deprecated.</p>
<p>To implement inheritance, use the
<a href="modules/sdk/core/heritage.html"><code>heritage</code></a>
module.</p>
</div>

[Traits](http://en.wikipedia.org/wiki/Trait_%28computer_science%29) are a simple
mechanism for structuring object-oriented programs. They represent reusable and
composable building blocks of functionality that factor out the common
attributes and behavior of objects.

They are a more robust alternative to
[mixins](http://en.wikipedia.org/wiki/Mixins) and
[multiple inheritance](http://en.wikipedia.org/wiki/Multiple_inheritance),
because name clashes must be explicitly resolved and composition is commutative
and associative (i.e. the order of traits in a composition is irrelevant).

Use traits to share functionality between similar objects without duplicating
code or creating complex inheritance chains.

## Trait Creation ##

To create a trait, call the `Trait` constructor function exported by this
module, passing it a JavaScript object that specifies the properties of the
trait.

    let Trait = require('light-traits').Trait;
    let t = Trait({
      foo: "foo",
      bar: function bar() {
        return "Hi!"
      },
      baz: Trait.required
    });

Traits can both provide and require properties. A *provided* property is a
property for which the trait itself provides a value. A *required* property is a
property that the trait needs in order to function correctly but for which
it doesn't provide a value.

Required properties must be provided by another trait or by an object with a
trait. Creation of an object with a trait will fail if required properties are
not provided. Specify a required property by setting the value of the property
to `Trait.required`.

## Object Creation ##

Create objects with a single trait by calling the trait's `create` method. The
method takes a single argument, the object to serve as the new object's
prototype. If no prototype is specified, the new object's prototype will be
`Object.prototype`.

    let t = Trait({
      foo: 'foo',
      bar: 2
    });
    let foo1 = t.create();
    let foo2 = t.create({ name: 'Super' });

## Trait Composition ##

Traits are designed to be composed with other traits to create objects with the
properties of multiple traits. To compose an object with multiple traits, you
first create a composite trait and then use it to create the object. A composite
trait is a trait that contains all of the properties of the traits from which it
is composed. In the following example, MagnitudeTrait is a composite trait.

    let EqualityTrait = Trait({
      equal: Trait.required,
      notEqual: function notEqual(x) {
        return !this.equal(x)
      }
    });
    
    let ComparisonTrait = Trait({
      less: Trait.required,
      notEqual: Trait.required,
      greater: function greater(x) {
        return !this.less(x) && this.notEqual(x)
      }
    });
    
    let MagnitudeTrait = Trait.compose(EqualityTrait, ComparisonTrait);

<?xml version="1.0"?>
<svg xmlns="http://www.w3.org/2000/svg" xmlns:xl="http://www.w3.org/1999/xlink" version="1.1" viewBox="-11 121 490 190" width="490px" height="190px">
  <defs>
    <marker orient="auto" overflow="visible" markerUnits="strokeWidth" id="SharpArrow_Marker" viewBox="-4 -4 10 8" markerWidth="10" markerHeight="8" color="black">
      <g>
        <path d="M 5 0 L -3 -3 L 0 0 L 0 0 L -3 3 Z" fill="currentColor" stroke="currentColor" stroke-width="1px"/>
      </g>
    </marker>
  </defs>
  <g stroke="none" stroke-opacity="1" stroke-dasharray="none" fill="none" fill-opacity="1">
    <g>
      <rect x="9" y="165.33334" width="141" height="14"/>
      <rect x="9" y="165.33334" width="141" height="14" stroke="black" stroke-width="1px"/>
      <text transform="translate(14 165.33334)" fill="black">
        <tspan font-family="Helvetica" font-size="12" font-weight="500" x="0" y="11" textLength="47.373047">notEqual</tspan>
      </text>
      <rect x="9" y="151.33334" width="141" height="14"/>
      <rect x="9" y="151.33334" width="141" height="14" stroke="black" stroke-width="1px"/>
      <text transform="translate(14 151.33334)" fill="red">
        <tspan font-family="Helvetica" font-size="12" font-weight="500" fill="red" x="0" y="11" textLength="29.361328">equal</tspan>
      </text>
      <rect x="9" y="137.33334" width="141" height="14"/>
      <rect x="9" y="137.33334" width="141" height="14" stroke="black" stroke-width="1px"/>
      <text transform="translate(14 137.33334)" fill="black">
        <tspan font-family="Helvetica" font-size="12" font-weight="bold" x="38.49707" y="11" textLength="54.00586">EqualityTrait</tspan>
      </text>
      <rect x="9" y="273" width="141" height="14"/>
      <rect x="9" y="273" width="141" height="14" stroke="black" stroke-width="1px"/>
      <text transform="translate(14 273)" fill="black">
        <tspan font-family="Helvetica" font-size="12" font-weight="500" x="0" y="11" textLength="38.021484">greater</tspan>
      </text>
      <rect x="9" y="259" width="141" height="14"/>
      <rect x="9" y="259" width="141" height="14" stroke="black" stroke-width="1px"/>
      <text transform="translate(14 259)" fill="red">
        <tspan font-family="Helvetica" font-size="12" font-weight="500" fill="red" x="0" y="11" textLength="47.373047">notEqual</tspan>
      </text>
      <rect x="9" y="245" width="141" height="14"/>
      <rect x="9" y="245" width="141" height="14" stroke="black" stroke-width="1px"/>
      <text transform="translate(14 245)" fill="red">
        <tspan font-family="Helvetica" font-size="12" font-weight="500" fill="red" x="0" y="11" textLength="21.339844">less</tspan>
      </text>
      <rect x="9" y="231" width="141" height="14"/>
      <rect x="9" y="231" width="141" height="14" stroke="black" stroke-width="1px"/>
      <text transform="translate(14 231)" fill="black">
        <tspan font-family="Helvetica" font-size="12" font-weight="bold" x=".15332031" y="11" textLength="112.67578">ComparisonTrait</tspan>
      </text>
      <rect x="317.75" y="235.5" width="141" height="14"/>
      <rect x="317.75" y="235.5" width="141" height="14" stroke="black" stroke-width="1px"/>
      <text transform="translate(322.75 235.5)" fill="black">
        <tspan font-family="Helvetica" font-size="12" font-weight="500" x="0" y="11" textLength="38.021484">greater</tspan>
      </text>
      <rect x="317.75" y="221.5" width="141" height="14"/> 
      <rect x="317.75" y="221.5" width="141" height="14" stroke="black" stroke-width="1px"/>
      <text transform="translate(322.75 221.5)" fill="red">
        <tspan font-family="Helvetica" font-size="12" font-weight="500" fill="red" x="0" y="11" textLength="21.339844">less</tspan>
      </text>
      <rect x="317.75" y="207.5" width="141" height="14"/>
      <rect x="317.75" y="207.5" width="141" height="14" stroke="black" stroke-width="1px"/>
      <text transform="translate(322.75 207.5)" fill="black">
        <tspan font-family="Helvetica" font-size="12" font-weight="500" x="0" y="11" textLength="47.373047">notEqual</tspan>
      </text>
      <rect x="317.75" y="193.5" width="141" height="14"/>
      <rect x="317.75" y="193.5" width="141" height="14" stroke="black" stroke-width="1px"/>
      <text transform="translate(322.75 193.5)" fill="red">
        <tspan font-family="Helvetica" font-size="12" font-weight="500" fill="red" x="0" y="11" textLength="29.361328">equal</tspan>
      </text>
      <rect x="317.75" y="179.5" width="141" height="14"/>
      <rect x="317.75" y="179.5" width="141" height="14" stroke="black" stroke-width="1px"/>
      <text transform="translate(322.75 179.5)" fill="black">
        <tspan font-family="Helvetica" font-size="12" font-weight="bold" x="31.83789" y="11" textLength="67.32422">MagnitudeTrait</tspan>
      </text>
      <path d="M 150 248.83887 L 158.89999 248.83887 L 235.9 248.83887 L 235.9 224.66113 L 308.85 224.66113 L 310.85 224.66113" marker-end="url(#SharpArrow_Marker)" stroke="black" stroke-linecap="butt" stroke-linejoin="miter" stroke-width="1px"/>
      <path d="M 150 171.15845 L 158.89999 171.15845 L 233.9 171.15845 L 233.9 201.6749 L 308.85 201.6749 L 310.85 201.6749" marker-end="url(#SharpArrow_Marker)" stroke="black" stroke-linecap="butt" stroke-linejoin="miter" stroke-width="1px"/>
    </g>
  </g>
</svg>

## Trait Resolution ##

Composite traits have conflicts when two of the traits in the composition
provide properties with the same name but different values (when compared using
the `===` strict equality operator). In the following example, `TC` has a
conflict because `T1` and `T2` both define a `foo` property:

    let T1 = Trait({
      foo: function () {
        // do something
      },
      bar: 'bar',
      t1: 1
    });
    
    let T2 = Trait({
      foo: function() {
        // do something else
      },
      bar: 'bar',
      t2: 2
    });
    
    let TC = Trait.compose(T1, T2);

Attempting to create an object from a composite trait with conflicts throws a
`remaining conflicting property` exception. To create objects from such traits,
you must resolve the conflict.

You do so by excluding or renaming the conflicting property of one of the
traits. Excluding a property removes it from the composition, so the composition
only acquires the property from the other trait. Renaming a property gives it a
new, non-conflicting name at which it can be accessed.

In both cases, you call the `resolve` method on the trait whose property you
want to exclude or rename, passing it an object. Each key in the object is the
name of a conflicting property; each value is either `null` to exclude the
property or a string representing the new name of the property.

For example, the conflict in the previous example could be resolved by excluding
the `foo` property of the second trait.

    let TC = Trait(T1, T2.resolve({ foo: null }));

It could also be resolved by renaming the `foo` property of the first trait to
`foo2`:

    let TC = Trait(T1.resolve({ foo: "foo2" }), T2);

When you resolve a conflict, the same-named property of the other trait (the one
that wasn't excluded or renamed) remains available in the composition under its
original name.

## Constructor Functions ##

When your code is going to create more than one object with traits, you may want
to define a constructor function to create them. To do so, create a composite
trait representing the traits the created objects should have, then define a
constructor function that creates objects with that trait and whose prototype is
the prototype of the constructor:

    let PointTrait = Trait.compose(T1, T2, T3);
    function Point(options) {
      let point = PointTrait.create(Point.prototype);
      return point;
    }

## Property Descriptor Maps ##

Traits are designed to work with the new object manipulation APIs defined in
[ECMAScript-262, Edition
5](http://www.ecma-international.org/publications/standards/Ecma-262.htm) (ES5).
Traits are also property descriptor maps that inherit from `Trait.prototype` to
expose methods for creating objects and resolving conflicts.

The following trait definition:

    let FooTrait = Trait({
      foo: "foo",
      bar: function bar() {
        return "Hi!"
      },
      baz: Trait.required
    });

Creates the following property descriptor map:

    {
      foo: {
        value: 'foo',
        enumerable: true,
        configurable: true,
        writable: true
      },
    
      bar: {
        value: function b() {
          return 'bar'
        },
        enumerable: true,
        configurable: true,
        writable: true
      },
    
      baz: {
        get baz() { throw new Error('Missing required property: `baz`') }
        set baz() { throw new Error('Missing required property: `baz`') }
      },
    
      __proto__: Trait.prototype
    }

Since Traits are also property descriptor maps, they can be used with built-in
`Object.*` methods that accept such maps:

    Object.create(proto, FooTrait);
    Object.defineProperties(myObject, FooTrait);

Note that conflicting and required properties won't cause exceptions to be
thrown when traits are used with the `Object.*` methods, since those methods are
not aware of those constraints. However, such exceptions will be thrown when the
property with the conflict or the required but missing property is accessed.

Property descriptor maps can also be used in compositions. This may be useful
for defining non-enumerable properties, for example:

    let TC = Trait.compose(
      Trait({ foo: 'foo' }),
      { bar: { value: 'bar', enumerable: false } }
    );

_When using property descriptor maps in this way, make sure the map is not the
only argument to `Trait.compose`, since in that case it will be interpreted as
an object literal with properties to be defined._

