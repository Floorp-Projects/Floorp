// |reftest| skip-if(!this.hasOwnProperty("Intl"))

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

var desc = Object.getOwnPropertyDescriptor(Intl.NumberFormat.prototype, Symbol.toStringTag);

assertEq(desc !== undefined, true);
assertEq(desc.value, "Object");
assertEq(desc.writable, false);
assertEq(desc.enumerable, false);
assertEq(desc.configurable, true);

assertEq(Object.prototype.toString.call(Intl.NumberFormat.prototype), "[object Object]");
assertEq(Object.prototype.toString.call(new Intl.NumberFormat), "[object Object]");

Object.defineProperty(Intl.NumberFormat.prototype, Symbol.toStringTag, {value: "NumberFormat"});

assertEq(Object.prototype.toString.call(Intl.NumberFormat.prototype), "[object NumberFormat]");
assertEq(Object.prototype.toString.call(new Intl.NumberFormat), "[object NumberFormat]");

delete Intl.NumberFormat.prototype[Symbol.toStringTag];

assertEq(Object.prototype.toString.call(Intl.NumberFormat.prototype), "[object Object]");
assertEq(Object.prototype.toString.call(new Intl.NumberFormat), "[object Object]");

if (typeof reportCompare === "function")
    reportCompare(true, true);
