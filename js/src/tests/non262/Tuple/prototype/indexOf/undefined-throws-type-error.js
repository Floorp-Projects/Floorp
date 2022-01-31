// |reftest| skip-if(!this.hasOwnProperty("Tuple"))
// Copyright (c) 2012 Ecma International.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
description: Tuple.prototype.indexOf applied to undefined throws a TypeError
---*/


assertThrowsInstanceOf(function() {
  Tuple.prototype.indexOf.call(undefined);
}, TypeError);

assertThrowsInstanceOf(function() {
  Tuple.prototype.indexOf.call(null);
}, TypeError);

reportCompare(0, 0);
