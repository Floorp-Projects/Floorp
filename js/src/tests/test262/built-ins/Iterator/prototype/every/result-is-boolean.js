// |reftest| shell-option(--enable-iterator-helpers) skip-if(!this.hasOwnProperty('Iterator')||!xulRuntime.shell) -- iterator-helpers is not enabled unconditionally, requires shell-options
// Copyright (C) 2023 Michael Ficarra. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.
/*---
esid: sec-iteratorprototype.every
description: >
  Iterator.prototype.every returns a boolean
features: [iterator-helpers]
---*/
function* g() {}
let iter = g();
assert.sameValue(typeof iter.every(() => {}), 'boolean');

reportCompare(0, 0);
