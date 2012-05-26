load(libdir + "asserts.js");

assertThrowsInstanceOf(function () {
    eval("function f(...rest=23) {}");
}, SyntaxError);
assertThrowsInstanceOf(function () {
    eval("function f(a=16, b) {}");
}, SyntaxError);
assertThrowsInstanceOf(function () {
    eval("function f([a]=4) {}");
}, SyntaxError);
assertThrowsInstanceOf(function () {
    eval("function f(a=4, [b]) {}");
}, SyntaxError);
assertThrowsInstanceOf(function () {
    eval("function f(a=yield 24) {}");
}, SyntaxError);
assertThrowsInstanceOf(function () {
    eval("function f(a={a : 19 + (yield 24).prop}) {}");
}, SyntaxError);
function silly_but_okay(a=(function () { yield 97; })) {}
