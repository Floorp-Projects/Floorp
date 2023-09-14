// |reftest| shell-option(--enable-array-grouping) skip-if(!Object.groupBy||!xulRuntime.shell) -- array-grouping is not enabled unconditionally, requires shell-options
// Copyright (c) 2023 Ecma International.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-map.groupby
description: Map.groupBy calls function with correct arguments
info: |
  Map.groupBy ( items, callbackfn )

  ...
  GroupBy ( items, callbackfn, coercion )

  6. Repeat,

      e. Let key be Completion(Call(callbackfn, undefined, « value, 𝔽(k) »)).
  ...
features: [array-grouping, Map]
---*/


const arr = [-0, 0, 1, 2, 3];

let calls = 0;

Map.groupBy(arr, function (n, i) {
  calls++;
  assert.sameValue(n, arr[i], "selected element aligns with index");
  assert.sameValue(arguments.length, 2, "only two arguments are passed");
  return null;
});

assert.sameValue(calls, 5, 'called for all 5 elements');

reportCompare(0, 0);
