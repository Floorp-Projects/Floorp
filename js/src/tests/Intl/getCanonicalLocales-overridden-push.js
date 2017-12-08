// |reftest| skip-if(!this.hasOwnProperty("Intl"))
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// Tests the getCanonicalLocales function for overriden Array.push.

Array.prototype.push = () => { throw 42; };

// must not throw 42, might if push is used
var arr = Intl.getCanonicalLocales(["en-US"]);

assertEqArray(arr, ["en-US"]);

if (typeof reportCompare === 'function')
    reportCompare(0, 0);
