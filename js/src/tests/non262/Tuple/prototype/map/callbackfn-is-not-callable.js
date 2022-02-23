// |reftest| skip-if(!this.hasOwnProperty("Tuple"))
// Copyright (C) 2016 the V8 project authors. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.
/*---
esid: sec-%Tuple%.prototype.map
description: >
  callbackfn arguments
info: |
  8.2.3.20 %Tuple%.prototype.map ( callbackfn [ , thisArg ] )

  ...
  4. If IsCallable(callbackfn) is false, throw a TypeError exception.
  ...
features: [Tuple]
---*/

var sample = #[1,2,3];

assertThrowsInstanceOf(() => sample.map(), TypeError,
                       "map: callbackfn is not callable (no argument)");

assertThrowsInstanceOf(() => sample.map(undefined), TypeError,
                       "map: callbackfn is not callable (undefined)");

assertThrowsInstanceOf(() => sample.map(null), TypeError,
                       "map: callbackfn is not callable (null)");

assertThrowsInstanceOf(() => sample.map({}), TypeError,
                       "map: callbackfn is not callable ({})");

assertThrowsInstanceOf(() => sample.map(1), TypeError,
                       "map: callbackfn is not callable (1)");

assertThrowsInstanceOf(() => sample.map(""), TypeError,
                       "map: callbackfn is not callable (\"\")");

assertThrowsInstanceOf(() => sample.map(false), TypeError,
                       "map: callbackfn is not callable (false)");

reportCompare(0, 0);
