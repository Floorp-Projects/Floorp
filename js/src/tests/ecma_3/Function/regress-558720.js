/*
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/licenses/publicdomain/
 */

var gTestfile = 'regress-558720.js';
var BUGNUMBER = 558720;
var summary = 'bad closure optimization inside "with"';
var actual;
var expect;

printBugNumber(BUGNUMBER);
printStatus(summary);

var a = new Function("a","b","with(a){return(function(){return b})()}");
var actual = a({b:"green"}) || "red";

eval("function a(a,b){with(a){return(function(){return b})()}}");
var expect = a({b:"green"}) || "red";

reportCompare(expect, actual, summary);

printStatus("All tests passed!");
