load(libdir + "asserts.js");

var x = 'global';
function f(a=x) {  // global variable x
    var x = 'local';
    return a;
}
assertEq(f(), 'global');

var i = 42;
function g(f=function () { return ++i; }) {  // closes on global variable i
    var i = 0;
    return f;
}
var gf = g();
assertEq(gf(), 43);
assertEq(gf(), 44);
gf = g();
assertEq(gf(), 45);

function h(f=function (s) { return eval(s); }) {  // closes on global scope
    var x = 'hlocal';
    return f;
}
var hf = h();
assertEq(hf('x'), 'global');
assertEq(hf('f'), hf);
assertEq(hf('var x = 3; x'), 3);

function j(expr, v=eval(expr)) {
  return v;
}
assertEq(j("expr"), "expr");
assertThrowsInstanceOf(() => j("v"), ReferenceError);
assertEq(j("Array"), Array);
assertEq(j("arguments").length, 1);
