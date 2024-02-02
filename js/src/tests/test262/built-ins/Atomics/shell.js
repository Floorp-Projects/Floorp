// GENERATED, DO NOT EDIT
// file: testAtomics.js
// Copyright (C) 2017 Mozilla Corporation. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.
/*---
description: |
    Collection of functions used to assert the correctness of SharedArrayBuffer objects.
defines:
  - testWithAtomicsOutOfBoundsIndices
  - testWithAtomicsInBoundsIndices
  - testWithAtomicsNonViewValues
---*/


/**
 * Calls the provided function for a each bad index that should throw a
 * RangeError when passed to an Atomics method on a SAB-backed view where
 * index 125 is out of range.
 *
 * @param f - the function to call for each bad index.
 */
function testWithAtomicsOutOfBoundsIndices(f) {
  var bad_indices = [
    function(view) { return -1; },
    function(view) { return view.length; },
    function(view) { return view.length * 2; },
    function(view) { return Number.POSITIVE_INFINITY; },
    function(view) { return Number.NEGATIVE_INFINITY; },
    function(view) { return { valueOf: function() { return 125; } }; },
    function(view) { return { toString: function() { return '125'; }, valueOf: false }; }, // non-callable valueOf triggers invocation of toString
  ];

  for (var i = 0; i < bad_indices.length; ++i) {
    var IdxGen = bad_indices[i];
    try {
      f(IdxGen);
    } catch (e) {
      e.message += ' (Testing with index gen ' + IdxGen + '.)';
      throw e;
    }
  }
}

/**
 * Calls the provided function for each good index that should not throw when
 * passed to an Atomics method on a SAB-backed view.
 *
 * The view must have length greater than zero.
 *
 * @param f - the function to call for each good index.
 */
function testWithAtomicsInBoundsIndices(f) {
  // Most of these are eventually coerced to +0 by ToIndex.
  var good_indices = [
    function(view) { return 0/-1; },
    function(view) { return '-0'; },
    function(view) { return undefined; },
    function(view) { return NaN; },
    function(view) { return 0.5; },
    function(view) { return '0.5'; },
    function(view) { return -0.9; },
    function(view) { return { password: 'qumquat' }; },
    function(view) { return view.length - 1; },
    function(view) { return { valueOf: function() { return 0; } }; },
    function(view) { return { toString: function() { return '0'; }, valueOf: false }; }, // non-callable valueOf triggers invocation of toString
  ];

  for (var i = 0; i < good_indices.length; ++i) {
    var IdxGen = good_indices[i];
    try {
      f(IdxGen);
    } catch (e) {
      e.message += ' (Testing with index gen ' + IdxGen + '.)';
      throw e;
    }
  }
}

/**
 * Calls the provided function for each value that should throw a TypeError
 * when passed to an Atomics method as a view.
 *
 * @param f - the function to call for each non-view value.
 */

function testWithAtomicsNonViewValues(f) {
  var values = [
    null,
    undefined,
    true,
    false,
    new Boolean(true),
    10,
    3.14,
    new Number(4),
    'Hi there',
    new Date,
    /a*utomaton/g,
    { password: 'qumquat' },
    new DataView(new ArrayBuffer(10)),
    new ArrayBuffer(128),
    new SharedArrayBuffer(128),
    new Error('Ouch'),
    [1,1,2,3,5,8],
    function(x) { return -x; },
    Symbol('halleluja'),
    // TODO: Proxy?
    Object,
    Int32Array,
    Date,
    Math,
    Atomics
  ];

  for (var i = 0; i < values.length; ++i) {
    var nonView = values[i];
    try {
      f(nonView);
    } catch (e) {
      e.message += ' (Testing with non-view value ' + nonView + '.)';
      throw e;
    }
  }
}


// file: testTypedArray.js
// Copyright (C) 2015 André Bargull. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.
/*---
description: |
    Collection of functions used to assert the correctness of TypedArray objects.
defines:
  - floatArrayConstructors
  - nonClampedIntArrayConstructors
  - intArrayConstructors
  - typedArrayConstructors
  - TypedArray
  - testWithTypedArrayConstructors
  - nonAtomicsFriendlyTypedArrayConstructors
  - testWithAtomicsFriendlyTypedArrayConstructors
  - testWithNonAtomicsFriendlyTypedArrayConstructors
  - testTypedArrayConversions
---*/

var floatArrayConstructors = [
  Float64Array,
  Float32Array
];

var nonClampedIntArrayConstructors = [
  Int32Array,
  Int16Array,
  Int8Array,
  Uint32Array,
  Uint16Array,
  Uint8Array
];

var intArrayConstructors = nonClampedIntArrayConstructors.concat([Uint8ClampedArray]);

// Float16Array is a newer feature
// adding it to this list unconditionally would cause implementations lacking it to fail every test which uses it
if (typeof Float16Array !== 'undefined') {
  floatArrayConstructors.push(Float16Array);
}

/**
 * Array containing every non-bigint typed array constructor.
 */

var typedArrayConstructors = floatArrayConstructors.concat(intArrayConstructors);

/**
 * The %TypedArray% intrinsic constructor function.
 */
var TypedArray = Object.getPrototypeOf(Int8Array);

/**
 * Callback for testing a typed array constructor.
 *
 * @callback typedArrayConstructorCallback
 * @param {Function} Constructor the constructor object to test with.
 */

/**
 * Calls the provided function for every typed array constructor.
 *
 * @param {typedArrayConstructorCallback} f - the function to call for each typed array constructor.
 * @param {Array} selected - An optional Array with filtered typed arrays
 */
function testWithTypedArrayConstructors(f, selected) {
  var constructors = selected || typedArrayConstructors;
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

var nonAtomicsFriendlyTypedArrayConstructors = floatArrayConstructors.concat([Uint8ClampedArray]);
/**
 * Calls the provided function for every non-"Atomics Friendly" typed array constructor.
 *
 * @param {typedArrayConstructorCallback} f - the function to call for each typed array constructor.
 * @param {Array} selected - An optional Array with filtered typed arrays
 */
function testWithNonAtomicsFriendlyTypedArrayConstructors(f) {
  testWithTypedArrayConstructors(f, nonAtomicsFriendlyTypedArrayConstructors);
}

/**
 * Calls the provided function for every "Atomics Friendly" typed array constructor.
 *
 * @param {typedArrayConstructorCallback} f - the function to call for each typed array constructor.
 * @param {Array} selected - An optional Array with filtered typed arrays
 */
function testWithAtomicsFriendlyTypedArrayConstructors(f) {
  testWithTypedArrayConstructors(f, [
    Int32Array,
    Int16Array,
    Int8Array,
    Uint32Array,
    Uint16Array,
    Uint8Array,
  ]);
}

/**
 * Helper for conversion operations on TypedArrays, the expected values
 * properties are indexed in order to match the respective value for each
 * TypedArray constructor
 * @param  {Function} fn - the function to call for each constructor and value.
 *                         will be called with the constructor, value, expected
 *                         value, and a initial value that can be used to avoid
 *                         a false positive with an equivalent expected value.
 */
function testTypedArrayConversions(byteConversionValues, fn) {
  var values = byteConversionValues.values;
  var expected = byteConversionValues.expected;

  testWithTypedArrayConstructors(function(TA) {
    var name = TA.name.slice(0, -5);

    return values.forEach(function(value, index) {
      var exp = expected[name][index];
      var initial = 0;
      if (exp === 0) {
        initial = 1;
      }
      fn(TA, value, exp, initial);
    });
  });
}

/**
 * Checks if the given argument is one of the float-based TypedArray constructors.
 *
 * @param {constructor} ctor - the value to check
 * @returns {boolean}
 */
function isFloatTypedArrayConstructor(arg) {
  return floatArrayConstructors.indexOf(arg) !== -1;
}

/**
 * Determines the precision of the given float-based TypedArray constructor.
 *
 * @param {constructor} ctor - the value to check
 * @returns {string} "half", "single", or "double" for Float16Array, Float32Array, and Float64Array respectively.
 */
function floatTypedArrayConstructorPrecision(FA) {
  if (typeof Float16Array !== "undefined" && FA === Float16Array) {
    return "half";
  } else if (FA === Float32Array) {
    return "single";
  } else if (FA === Float64Array) {
    return "double";
  } else {
    throw new Error("Malformed test - floatTypedArrayConstructorPrecision called with non-float TypedArray");
  }
}
