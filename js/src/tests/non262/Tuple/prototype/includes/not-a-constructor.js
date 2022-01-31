// |reftest| skip-if(!this.hasOwnProperty("Tuple"))
// Copyright (C) 2020 Rick Waldron. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
description: >
  Tuple.prototype.includes does not implement [[Construct]], is not new-able
---*/

assertEq(
  isConstructor(Tuple.prototype.includes),
  false,
  'isConstructor(Tuple.prototype.includes) must return false'
);

assertThrowsInstanceOf(() => new Tuple.prototype.includes(1),
		       TypeError, '`new Tuple.prototype.includes(1)` throws TypeError');


reportCompare(0, 0);
