load(libdir + "asserts.js");

eval(`"use strict";
function f1(f=(function () { return typeof this !== "object"; })) { return f; }
assertEq(f1()(), true);
`);

function f2(f=(function () { "use strict"; return (function () { return typeof this !== "object"; }) })) { assertEq(typeof this, "object"); return f; }
assertEq(f2()()(), true);

eval(`"use strict";
function f3(f=(function () { return (function () { return typeof this !== "object"; }) })) { return f; }
assertEq(f3()()(), true);
`);

// These should be okay.
function f4(f=(function () { with (Object) {} }), g=(function () { "use strict"; })) {}
function f5(g=(function () { "use strict"; }), f=(function () { with (Object) {} })) {}

assertThrowsInstanceOf(function () {
    eval("'use strict'; function f(a=delete x) { }");
}, SyntaxError);
assertThrowsInstanceOf(function () {
    Math.sin(4);
    eval("'use strict'; function f(a='\\251') { }");
}, SyntaxError);
assertThrowsInstanceOf(function () {
    eval("'use strict'; function f(a='\\251', b=delete x) { }");
}, SyntaxError);
assertThrowsInstanceOf(function () {
    eval("'use strict'; function f(a=delete x, b='\\251') { }");
}, SyntaxError);
assertThrowsInstanceOf(function () {
    eval("'use strict'; function f(a=(function () { '\\251'; })) { }");
}, SyntaxError);
assertThrowsInstanceOf(function () {
    eval("'use strict'; function f(a=(function () { with (Object) {} })) { }");
}, SyntaxError);
assertThrowsInstanceOf(function () {
    eval("'use strict'; function f(a=(function (b, b) {})) { }");
}, SyntaxError);
