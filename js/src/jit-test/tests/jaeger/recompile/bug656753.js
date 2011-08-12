
var global = 0;

function foo(i) {
  global = i;
  if (global == 8) {
    eval("global = 'three'");
    throw global;
  }
}

var caught = false;
try {
  for (var i = 0; i < 10; i++) {
    Array.map([i], foo);
 }
} catch (e) { caught = true; }

assertEq(caught, true);
assertEq(global, 'three');
