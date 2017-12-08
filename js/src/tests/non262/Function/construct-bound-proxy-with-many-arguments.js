/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

var proxy = Function.prototype.bind.call(new Proxy(Array, {}));
for (var i = 10; i < 50; ++i) {
    var args = Array(i).fill(i);
    var array = new proxy(...args);
    assertEqArray(array, args);
}

if (typeof reportCompare === "function")
    reportCompare(0, 0, "ok");
