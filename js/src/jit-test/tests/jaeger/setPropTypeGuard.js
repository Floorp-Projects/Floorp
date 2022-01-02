
/*
 * Get a SETPROP site which is monitored (unknown lhs) and is repeatedly
 * invoked on objects with the same shape but different types (and without
 * triggering a recompile of the function). The SETPROP PIC needs a type guard
 * when the object is being monitored.
 */
var x = {g:0};
var y = {g:0,f:"fubar"};
x.f = 10;

function foo(x) {
  for (var i = 0; i < 30; i++)
    x.f = 10;
}
function access(x) {
  return x.f + 10;
}
foo(Object.create({}));
eval("foo(x)");
assertEq(access(y), "fubar10");
eval("foo(y)");
assertEq(access(y), 20);
