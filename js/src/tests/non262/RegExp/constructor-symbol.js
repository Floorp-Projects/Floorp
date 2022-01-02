/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

for (let sym of [Symbol.iterator, Symbol(), Symbol("description")]) {
    assertThrowsInstanceOf(() => RegExp(sym), TypeError);
    assertThrowsInstanceOf(() => new RegExp(sym), TypeError);

    assertThrowsInstanceOf(() => RegExp(sym, "g"), TypeError);
    assertThrowsInstanceOf(() => new RegExp(sym, "g"), TypeError);
}

if (typeof reportCompare === 'function')
    reportCompare(0, 0);
