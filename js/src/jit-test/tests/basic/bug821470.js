load(libdir + "asserts.js");

assertThrowsInstanceOf(function () {
    eval("function x() { 'use strict'; const x = 4; x = 3; }");
}, SyntaxError);
assertThrowsInstanceOf(function () {
    Function("'use strict'; const x = x = 5;");
}, SyntaxError)
