// |reftest| skip-if(!this.hasOwnProperty("Intl"))

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

var desc = Object.getOwnPropertyDescriptor(Intl.DateTimeFormat.prototype, Symbol.toStringTag);

assertEq(desc !== undefined, true);
assertEq(desc.value, "Object");
assertEq(desc.writable, false);
assertEq(desc.enumerable, false);
assertEq(desc.configurable, true);

assertEq(Object.prototype.toString.call(Intl.DateTimeFormat.prototype), "[object Object]");
assertEq(Object.prototype.toString.call(new Intl.DateTimeFormat), "[object Object]");

Object.defineProperty(Intl.DateTimeFormat.prototype, Symbol.toStringTag, {value: "DateTimeFormat"});

assertEq(Object.prototype.toString.call(Intl.DateTimeFormat.prototype), "[object DateTimeFormat]");
assertEq(Object.prototype.toString.call(new Intl.DateTimeFormat), "[object DateTimeFormat]");

delete Intl.DateTimeFormat.prototype[Symbol.toStringTag];

assertEq(Object.prototype.toString.call(Intl.DateTimeFormat.prototype), "[object Object]");
assertEq(Object.prototype.toString.call(new Intl.DateTimeFormat), "[object Object]");

if (typeof reportCompare === "function")
    reportCompare(true, true);
