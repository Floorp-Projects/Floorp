// |reftest| shell-option(--enable-iterator-helpers) skip-if(!this.hasOwnProperty('Iterator')||!xulRuntime.shell) -- iterator-helpers is not enabled unconditionally, requires shell-options
// Copyright (C) 2020 Rick Waldron. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.
/*---
esid: sec-iteratorprototype.reduce
description: >
  Iterator.prototype.reduce is callable
features: [iterator-helpers]
---*/
function* g() {}
Iterator.prototype.reduce.call(g(), () => {}, 0);

let iter = g();
iter.reduce(() => {}, 0);

reportCompare(0, 0);
