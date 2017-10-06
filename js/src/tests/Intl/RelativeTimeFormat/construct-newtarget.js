// |reftest| skip-if(!this.hasOwnProperty("Intl")||!this.hasOwnProperty("addIntlExtras"))

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

addIntlExtras(Intl);

var obj = new Intl.RelativeTimeFormat();

// Test that new RTF produces an object with the right prototype.
assertEq(Object.getPrototypeOf(obj), Intl.RelativeTimeFormat.prototype);

// Test subclassing %Intl.RelativeTimeFormat% works correctly.
class MyRelativeTimeFormat extends Intl.RelativeTimeFormat {}

var obj = new MyRelativeTimeFormat();
assertEq(obj instanceof MyRelativeTimeFormat, true);
assertEq(obj instanceof Intl.RelativeTimeFormat, true);
assertEq(Object.getPrototypeOf(obj), MyRelativeTimeFormat.prototype);

if (typeof reportCompare === "function")
    reportCompare(0, 0);
