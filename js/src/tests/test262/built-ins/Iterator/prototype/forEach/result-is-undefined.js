// |reftest| shell-option(--enable-iterator-helpers) skip-if(!this.hasOwnProperty('Iterator')||!xulRuntime.shell) -- iterator-helpers is not enabled unconditionally, requires shell-options
// Copyright (C) 2023 Michael Ficarra. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.
/*---
esid: sec-iteratorprototype.forEach
description: >
  Iterator.prototype.forEach returns undefined
features: [iterator-helpers]
---*/
function* g() {}
let iter = g();
assert.sameValue(
  iter.forEach(() => {}),
  undefined
);
assert.sameValue(
  iter.forEach(() => 0),
  undefined
);

reportCompare(0, 0);
