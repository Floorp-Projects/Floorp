/*
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/licenses/publicdomain/
 */

var gTestfile = '7.8.5-1.js';
var BUGNUMBER = 98409;
var summary = 'regular expression literal evaluates to new RegExp instance';

printBugNumber(BUGNUMBER);
printStatus(summary);

function flip(x) {
    return /hi/g.test(x);
}
function flop(x) {
    return r.test(x);
}
var r = /hi/g;
var s = "hi there";

var actual = [flip(s), flip(s), flip(s), flip(s), flop(s), flop(s), flop(s), flop(s)].join(',');
var expect = "true,true,true,true,true,false,true,false";

reportCompare(expect, actual, summary);

printStatus("All tests passed!");
