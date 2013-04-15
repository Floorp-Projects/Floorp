<!-- This Source Code Form is subject to the terms of the Mozilla Public
   - License, v. 2.0. If a copy of the MPL was not distributed with this
   - file, You can obtain one at http://mozilla.org/MPL/2.0/. -->

The `util/object` module provides simple helper functions for working with 
objects.

<api name="merge">
@function
Merges all of the properties of all arguments into the first argument. If
two or more argument objects have properties with the same name, the 
property is overwritten with precedence from right to left, implying that
properties of the object on the left are overwritten by a same named property
of an object on the right. Properties are merged with [descriptors](https://developer.mozilla.org/en-US/docs/JavaScript/Reference/Global_Objects/Object/defineProperty#Description) onto the source object.

Any argument given with "falsy" values (null, undefined) in case of objects
are skipped.

    let { merge } = require("sdk/util/object");
    var a = { jetpacks: "are yes", foo: 10 }
    var b = merge(a, { foo: 5, bar: 6 }, { foo: 50, location: "SF" });

    b === a    // true
    b.jetpacks // "are yes"
    b.foo      // 50
    b.bar      // 6
    b.location // "SF"
  
    // Merge also translates property descriptors
    var c = { "type": "addon" };
    var d = {};
    Object.defineProperty(d, "name", {
      value: "jetpacks",
      configurable: false
    });
    merge(c, d);
    var props = Object.getOwnPropertyDescriptor(c, "name");
    console.log(props.configurable); // true
    
@param source {object}
  The object that other properties are merged into.

@param arguments {object}
  `n` amount of additional objects that are merged into `source` object.

@returns {object}
  The `source` object.
</api>

<api name="extend">
@function
Returns an object that inherits from the first argument and contains
all of the properties from all following arguments, with precedence from
right to left.

`extend(source1, source2, source3)` is the equivalent of
`merge(Object.create(source1), source2, source3)`.

    let { extend } = require("sdk/util/object");
    var a = { alpha: "a" };
    var b = { beta: "b" };
    var g = { gamma: "g", alpha: null };
    var x = extend(a, b, g);

    console.log(a); // { alpha: "a" }
    console.log(b); // { beta: "b" }
    console.log(g); // { gamma: "g", alpha: null }
    console.log(x); // { alpha: null, beta: "b", gamma: "g" }

@param arguments {object}
  `n` arguments that get merged into a new object.

@returns {object}
  The new, merged object.
</api>
