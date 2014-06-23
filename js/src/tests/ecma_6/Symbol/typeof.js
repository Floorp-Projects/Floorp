/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/licenses/publicdomain/ */

assertEq(typeof Symbol(), "symbol");
assertEq(typeof Symbol("ponies"), "symbol");

if (typeof reportCompare === "function")
    reportCompare(0, 0);
