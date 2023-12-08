/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
function run_test()
{
  if (!Services.prefs.getBoolPref("javascript.options.experimental.symbols_as_weakmap_keys")) {
    return;
  }

  var strKey = new String("strKey");
  var symKey = Symbol("symKey");

  var weakset = new WeakSet([strKey, symKey]);
  var weakmap = new WeakMap();
  weakmap.set(strKey, 23);
  weakmap.set(symKey, "oh no");

  var keys = ChromeUtils.nondeterministicGetWeakMapKeys(weakmap);
  equal(keys.length, 2, "length of nondeterministicGetWeakMapKeys");
  equal(weakmap.get(strKey), 23, "check strKey in weakmap");
  equal(weakmap.get(symKey), "oh no", "check symKey in weakmap");

  keys = ChromeUtils.nondeterministicGetWeakSetKeys(weakset);
  equal(keys.length, 2, "length of nondeterministicGetWeakSetKeys");
  ok(weakset.has(strKey), "check strKey in weakset");
  ok(weakset.has(symKey), "check symKey in weakset");

  strKey = null;
  keys = null;

  Cu.forceGC();

  keys = ChromeUtils.nondeterministicGetWeakMapKeys(weakmap);
  equal(keys.length, 1, "length of nondeterministicGetWeakMapKeys after GC");
  equal(weakmap.get(symKey), "oh no", "check symKey still in weakmap");

  keys = ChromeUtils.nondeterministicGetWeakSetKeys(weakset);
  equal(keys.length, 1, "length of nondeterministicGetWeakSetKeys after GC");
  ok(weakset.has(symKey), "check symKey still in weakset");
}
