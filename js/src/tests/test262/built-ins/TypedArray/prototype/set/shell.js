// GENERATED, DO NOT EDIT
// file: isConstructor.js
// Copyright (C) 2017 André Bargull. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
description: |
    Test if a given function is a constructor function.
defines: [isConstructor]
features: [Reflect.construct]
---*/

function isConstructor(f) {
    if (typeof f !== "function") {
      throw new Test262Error("isConstructor invoked with a non-function value");
    }

    try {
        Reflect.construct(function(){}, [], f);
    } catch (e) {
        return false;
    }
    return true;
}

// file: testBigIntTypedArray.js
// Copyright (C) 2015 André Bargull. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.
/*---
description: |
    Collection of functions used to assert the correctness of BigInt TypedArray objects.
defines:
  - TypedArray
  - testWithBigIntTypedArrayConstructors
---*/

/**
 * The %TypedArray% intrinsic constructor function.
 */
var TypedArray = Object.getPrototypeOf(Int8Array);

/**
 * Calls the provided function for every typed array constructor.
 *
 * @param {typedArrayConstructorCallback} f - the function to call for each typed array constructor.
 * @param {Array} selected - An optional Array with filtered typed arrays
 */
function testWithBigIntTypedArrayConstructors(f, selected) {
  /**
   * Array containing every BigInt typed array constructor.
   */
  var constructors = selected || [
    BigInt64Array,
    BigUint64Array
  ];

  for (var i = 0; i < constructors.length; ++i) {
    var constructor = constructors[i];
    try {
      f(constructor);
    } catch (e) {
      e.message += " (Testing with " + constructor.name + ".)";
      throw e;
    }
  }
}
