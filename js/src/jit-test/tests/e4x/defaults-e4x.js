load(libdir + "asserts.js");

assertThrowsInstanceOf(function () {
    eval("function f(a=<x/>) {}");
}, SyntaxError);
assertThrowsInstanceOf(function () {
    eval("function f(a=<x/>) { 'use strict'; }");
}, SyntaxError);
assertThrowsInstanceOf(function () {
    eval("function f(a=(function () { <x/> })) {}");
}, SyntaxError);
assertThrowsInstanceOf(function () {
    eval("function f(a=(function () { <x/> })) { 'use strict'; }");
}, SyntaxError);
