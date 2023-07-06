// |reftest| shell-option(--enable-iterator-helpers) skip-if(!this.hasOwnProperty('Iterator')||!xulRuntime.shell) -- iterator-helpers is not enabled unconditionally, requires shell-options
// Copyright (C) 2020 Rick Waldron. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.
/*---
esid: sec-iteratorprototype.map
description: >
  Iterator.prototype.map is callable
features: [iterator-helpers]
---*/
function* g() {}
Iterator.prototype.map.call(g(), () => 0);

let iter = g();
iter.map(() => 0);

reportCompare(0, 0);
