/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* See https://bugzilla.mozilla.org/show_bug.cgi?id=1165807 */

function run_test()
{
  var bunnies = new String("bunnies");
  var lizards = new String("lizards");

  var weakset = new WeakSet([bunnies, lizards]);
  var weakmap = new WeakMap();
  weakmap.set(bunnies, 23);
  weakmap.set(lizards, "oh no");

  var keys = ThreadSafeChromeUtils.nondeterministicGetWeakMapKeys(bunnies);
  equal(keys, undefined, "test nondeterministicGetWeakMapKeys on non-WeakMap");

  keys = ThreadSafeChromeUtils.nondeterministicGetWeakMapKeys(weakmap);
  equal(keys.length, 2, "length of nondeterministicGetWeakMapKeys");
  equal(weakmap.get(bunnies), 23, "check bunnies in weakmap");
  equal(weakmap.get(lizards), "oh no", "check lizards in weakmap");

  keys = ThreadSafeChromeUtils.nondeterministicGetWeakSetKeys(bunnies);
  equal(keys, undefined, "test nondeterministicGetWeakSetKeys on non-WeakMap");

  keys = ThreadSafeChromeUtils.nondeterministicGetWeakSetKeys(weakset);
  equal(keys.length, 2, "length of nondeterministicGetWeakSetKeys");
  ok(weakset.has(bunnies), "check bunnies in weakset");
  ok(weakset.has(lizards), "check lizards in weakset");

  bunnies = null;
  keys = null;

  Components.utils.forceGC();

  keys = ThreadSafeChromeUtils.nondeterministicGetWeakMapKeys(weakmap);
  equal(keys.length, 1, "length of nondeterministicGetWeakMapKeys after GC");
  equal(weakmap.get(lizards), "oh no", "check lizards still in weakmap");

  keys = ThreadSafeChromeUtils.nondeterministicGetWeakSetKeys(weakset);
  equal(keys.length, 1, "length of nondeterministicGetWeakSetKeys after GC");
  ok(weakset.has(lizards), "check lizards still in weakset");
}
