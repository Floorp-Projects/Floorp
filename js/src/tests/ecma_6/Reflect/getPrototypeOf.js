/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/licenses/publicdomain/ */

// Reflect.getPrototypeOf returns an object's prototype.
assertEq(Reflect.getPrototypeOf({}), Object.prototype);
assertEq(Reflect.getPrototypeOf(Object.prototype), null);
assertEq(Reflect.getPrototypeOf(Object.create(null)), null);

// Sleeper test for when scripted proxies support the getPrototypeOf handler
// method (bug 888969).
var proxy = new Proxy({}, {
    getPrototypeOf(t) { return Math; }
});
var result = Reflect.getPrototypeOf(proxy);
if (result === Math) {
    throw new Error("Congratulations on fixing bug 888969! " +
                    "Please update this test to cover scripted proxies.");
}

// For more Reflect.getPrototypeOf tests, see target.js.

reportCompare(0, 0);
