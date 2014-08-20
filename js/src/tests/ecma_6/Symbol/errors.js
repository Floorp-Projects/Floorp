/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/licenses/publicdomain/ */

if (typeof Symbol === "function") {
    // Section numbers cite ES6 rev 24 (2014 April 27).

    var sym = Symbol();

    // 7.2.2 IsCallable
    assertThrowsInstanceOf(() => sym(), TypeError);
    assertThrowsInstanceOf(() => Function.prototype.call.call(sym), TypeError);

    // 7.2.5 IsConstructor
    assertThrowsInstanceOf(() => new sym(), TypeError);
    assertThrowsInstanceOf(() => new Symbol(), TypeError);
}

if (typeof reportCompare === "function")
    reportCompare(0, 0);
