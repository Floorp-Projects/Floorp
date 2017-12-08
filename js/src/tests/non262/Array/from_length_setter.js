/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/licenses/publicdomain/ */

// Array.from calls a length setter if present.
var hits = 0;
function C() {}
C.prototype = {set length(v) { hits++; }};
C.from = Array.from;
var copy = C.from(["A", "B"]);
assertEq(hits, 1);

if (typeof reportCompare === 'function')
    reportCompare(0, 0);
