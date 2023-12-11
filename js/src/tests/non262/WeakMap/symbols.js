// |reftest| shell-option(--enable-symbols-as-weakmap-keys) skip-if(!xulRuntime.shell||!(this.hasOwnProperty('getBuildConfiguration')&&!getBuildConfiguration('release_or_beta'))) -- requires shell-options
/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/licenses/publicdomain/ */

var m = new WeakMap;
var sym = Symbol();
m.set(sym, 0);
assertEq(m.get(sym), 0);

// sym1 will be registered in global Symbol registry hence cannot be used as a
// key in WeakMap.
var sym1 = Symbol.for("testKey");
assertThrowsInstanceOf(() => m.set(sym1, 1), TypeError);

// Well-known symbols can be used as weakmap keys.
var sym2 = Symbol.hasInstance;
m.set(sym2, 2);
assertEq(m.get(sym2), 2);

if (typeof reportCompare === "function")
  reportCompare(0, 0);
