// |reftest| skip-if(!Function.prototype.toSource)
/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/licenses/publicdomain/ */

let x = {};
let y = function() {};
y.toSource = function() {
    return "[012345678901234567890123456789]";
};
Object.defineProperty(x, "", {enumerable: true, get: y});
assertEq(x.toSource(), "({'':[012345678901234567890123456789]})");

if (typeof reportCompare === "function")
    reportCompare(0, 0);
