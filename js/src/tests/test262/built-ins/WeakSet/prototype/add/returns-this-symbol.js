// |reftest| shell-option(--enable-symbols-as-weakmap-keys) skip-if(release_or_beta||!xulRuntime.shell) -- symbols-as-weakmap-keys is not released yet, requires shell-options
// Copyright (C) 2022 Igalia S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-weakset.prototype.add
description: Returns `this` after adding a new value.
info: |
  WeakSet.prototype.add ( value )

  1. Let S be the this value.
  ...
  7. Return S.
features: [Symbol, WeakSet, symbols-as-weakmap-keys]
---*/

var s = new WeakSet();

assert.sameValue(s.add(Symbol('description')), s, '`s.add(Symbol("description"))` returns `s`');

reportCompare(0, 0);
