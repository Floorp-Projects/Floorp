// Copyright (C) 2016 the V8 project authors. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.
/*---
esid: sec-integer-indexed-exotic-objects-defineownproperty-p-desc
description: >
  Returns true after setting a valid numeric index key
info: |
  9.4.5.3 [[DefineOwnProperty]] ( P, Desc)
  ...
  3. If Type(P) is String, then
    a. Let numericIndex be ! CanonicalNumericIndexString(P).
    b. If numericIndex is not undefined, then
      If ! IsValidIntegerIndex(O, numericIndex) is false, return false.
      If IsAccessorDescriptor(Desc) is true, return false.
      If Desc has a [[Configurable]] field and if Desc.[[Configurable]] is false, return false.
      If Desc has an [[Enumerable]] field and if Desc.[[Enumerable]] is false, return false.
      If Desc has a [[Writable]] field and if Desc.[[Writable]] is false, return false.
      If Desc has a [[Value]] field, then
        Let value be Desc.[[Value]].
        Return ? IntegerIndexedElementSet(O, numericIndex, value).

includes: [testTypedArray.js, propertyHelper.js]
features: [Reflect, TypedArray]
---*/

testWithTypedArrayConstructors(function(TA) {
  var sample = new TA([42, 42]);

  assert.sameValue(
    Reflect.defineProperty(sample, "0", {
      value: 8,
      configurable: true,
      enumerable: true,
      writable: true
    }),
    true
  );

  assert.sameValue(sample[0], 8, "property value was set");
  var desc = Object.getOwnPropertyDescriptor(sample, "0");

  assert.sameValue(desc.value, 8, "desc.value");
  assert.sameValue(desc.writable, true, "property is writable");

  verifyEnumerable(sample, "0");
  verifyNotConfigurable(sample, "0");
});

reportCompare(0, 0);
