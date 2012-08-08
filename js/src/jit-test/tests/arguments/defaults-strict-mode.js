load(libdir + "asserts.js");

function f1(f=(function () { return typeof this !== "object"; })) { "use strict"; return f; }
assertEq(f1()(), true);
function f2(f=(function () { "use strict"; return (function () { return typeof this !== "object"; }) })) { assertEq(typeof this, "object"); return f; }
assertEq(f2()()(), true);
function f3(f=(function () { return (function () { return typeof this !== "object"; }) })) { "use strict"; return f; }
assertEq(f3()()(), true);
// These should be okay.
function f4(f=(function () { with (Object) {} }), g=(function () { "use strict"; })) {}
function f5(g=(function () { "use strict"; }), f=(function () { with (Object) {} })) {}
function f6(f=(function () { return (x for (y in (function g() {}))); })) {}

assertThrowsInstanceOf(function () {
    eval("function f(a=delete x) { 'use strict'; }");
}, SyntaxError);
assertThrowsInstanceOf(function () {
    Math.sin(4);
    eval("function f(a='\\251') { 'use strict'; }");
}, SyntaxError);
assertThrowsInstanceOf(function () {
    eval("function f(a='\\251', b=delete x) { 'use strict'; }");
}, SyntaxError);
assertThrowsInstanceOf(function () {
    eval("function f(a=delete x, b='\\251') { 'use strict'; }");
}, SyntaxError);
assertThrowsInstanceOf(function () {
    eval("function f(a=(function () { '\\251'; })) { 'use strict'; }");
}, SyntaxError);
assertThrowsInstanceOf(function () {
    eval("function f(a=(function () { with (Object) {} })) { 'use strict'; }");
}, SyntaxError);
assertThrowsInstanceOf(function () {
    eval("function f(a=(function (b, b) {})) { 'use strict'; }");
}, SyntaxError);
