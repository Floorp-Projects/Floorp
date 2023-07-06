// |reftest| shell-option(--enable-iterator-helpers) skip-if(!this.hasOwnProperty('Iterator')||!xulRuntime.shell) -- iterator-helpers is not enabled unconditionally, requires shell-options
// Copyright (C) 2023 Michael Ficarra. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.
/*---
esid: sec-iteratorprototype.some
description: >
  Iterator.prototype.some returns a boolean
features: [iterator-helpers]
---*/
function* g() {}
let iter = g();
assert.sameValue(typeof iter.some(() => {}), 'boolean');

reportCompare(0, 0);
