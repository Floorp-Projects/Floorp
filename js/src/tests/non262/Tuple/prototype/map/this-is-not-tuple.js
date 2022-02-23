// |reftest| skip-if(!this.hasOwnProperty("Tuple"))
// Copyright (C) 2016 the V8 project authors. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.
/*---
esid: sec-%typedarray%.prototype.map
description: Throws a TypeError exception when `this` is not a Tuple
info: |

features: [Tuple]
---*/

var map = Tuple.prototype.map;
var callbackfn = function() {};

var thisVals = [[undefined, "undefined"],
                [null, "null"],
                [42, "42"],
                ["1", "1"],
                [true, "true"],
                [false, "false"],
                [Symbol("s"), "Symbol(\"s\")"]];

for (pair of thisVals) {
    thisVal = pair[0];
    errorMsg = "this is: " + pair[1];
    assertThrowsInstanceOf(() => map.call(thisVal, callbackfn),
                           TypeError, errorMsg);
}

reportCompare(0, 0);
