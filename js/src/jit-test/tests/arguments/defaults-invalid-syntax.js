load(libdir + "asserts.js");

assertThrowsInstanceOf(function () {
    eval("function f(...rest=23) {}");
}, SyntaxError);
assertThrowsInstanceOf(function () {
    eval("function f(a=yield 24) {}");
}, SyntaxError);
assertThrowsInstanceOf(function () {
    eval("function f(a={a : 19 + (yield 24).prop}) {}");
}, SyntaxError);
assertThrowsInstanceOf(function () {
    eval("function f(a=1,a=1) {}");
}, SyntaxError);
assertThrowsInstanceOf(function () {
    eval("function f(a,a=1) {}");
}, SyntaxError);
assertThrowsInstanceOf(function () {
    eval("function f(a=1,a) {}");
}, SyntaxError);
assertThrowsInstanceOf(function () {
    eval("function f(a,a,b=1) {}");
}, SyntaxError);
assertThrowsInstanceOf(function () {
    eval("function f(a,b=1,a=1) {}");
}, SyntaxError);
assertThrowsInstanceOf(function () {
    eval("function f(a=1,b=1,a=1) {}");
}, SyntaxError);
function silly_but_okay(a=(function* () { yield 97; })) {}
