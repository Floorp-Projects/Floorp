// |reftest| skip-if(!this.hasOwnProperty("Intl"))
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// Tests the getCanonicalLocales function for overridden argument's length.

var count = 0;
var locs = { get length() { if (count++ > 0) throw 42; return 0; } };
var locales = Intl.getCanonicalLocales(locs); // shouldn't throw 42
assertEq(locales.length, 0);


var obj = { get 0() { throw new Error("must not be gotten!"); },
            length: -Math.pow(2, 32) + 1 };

assertEq(Intl.getCanonicalLocales(obj).length, 0);

if (typeof reportCompare === 'function')
    reportCompare(0, 0);
