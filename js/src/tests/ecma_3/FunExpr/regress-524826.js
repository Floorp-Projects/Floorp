/*
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/licenses/publicdomain/
 */

var BUGNUMBER = 524826;
var summary = 'null-closure property initialiser mis-brands object literal scope';
var actual;
var expect;

printBugNumber(BUGNUMBER);
printStatus(summary);

function make(g) {
    var o = {f: function(a,b) { return a*b; }, g: g};
    return o;
}
var z = -1;
var x = make(function(c) { return c*z; });
var y = make(function(c) { return -c*z; });

function callg(o, c) { return o.g(c); };
actual = callg(x, 1);
expect = -callg(y, 1);

reportCompare(expect, actual, summary);

printStatus("All tests passed!");
