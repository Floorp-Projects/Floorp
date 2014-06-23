/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/licenses/publicdomain/ */

// More tests will be added here when Symbol.prototype.toString is added.

assertEq(Object.getPrototypeOf(Symbol.prototype), Object.prototype);

if (typeof reportCompare === "function")
    reportCompare(0, 0);
