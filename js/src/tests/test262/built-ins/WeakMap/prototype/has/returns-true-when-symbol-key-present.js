// |reftest| shell-option(--enable-symbols-as-weakmap-keys) skip-if(!(this.hasOwnProperty('getBuildConfiguration')&&!getBuildConfiguration('release_or_beta'))||!xulRuntime.shell) -- symbols-as-weakmap-keys is not enabled unconditionally, requires shell-options
// Copyright (C) 2022 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.
/*---
esid: sec-weakmap.prototype.has
description: >
  Returns true when an Object key is present in the WeakMap entries list.
info: |
  WeakMap.prototype.has ( _key_ )
  5. For each Record {[[Key]], [[Value]]} _p_ of _entries_, do
    a. If _p_.[[Key]] is not ~empty~ and SameValue(_p_.[[Key]], _key_) is
      *true*, return *true*.
features: [Symbol, WeakMap, symbols-as-weakmap-keys]
---*/

var foo = Symbol('a description');
var bar = Symbol('a description');
var map = new WeakMap();

map.set(foo, 1);
map.set(bar, 2);
assert.sameValue(map.has(foo), true, "Regular symbol as key");

map.delete(foo);
assert.sameValue(map.has(bar), true, "Symbols with the same description don't alias each other");

map.set(Symbol.hasInstance, 3);
assert.sameValue(map.has(Symbol.hasInstance), true, "Well-known symbol as key");

reportCompare(0, 0);
