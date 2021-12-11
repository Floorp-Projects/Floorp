// rejoining after recompilation from CompileFunction or a native called by an inlined frame.

var global = 0;
var arr = [
           function() { return 0; },
           function() { return 1; },
           function() { return 2; },
           function() { return 3; },
           function() { return 4; },
           function() { return 5; },
           function() { return 6; },
           function() { return 7; },
           function() { global = -"three"; return 8; }
           ];
function wrap_call(i) {
  var fn = arr["" + i];
  return fn();
}
function foo1() {
  var res = 0;
  for (var i = 0; i < arr.length; i++) {
    res += wrap_call(i);
    var expected = (i == arr.length - 1) ? NaN : 10;
    assertEq(global + 10, expected);
  }
}
foo1();

var callfn = Function.call;
function wrap_floor(x, y) {
  var z = x;
  if (y)
    z = {}; // trick the compiler into not inlining the floor() call.
  return Math.floor(z);
}

function foo2(x, y) {
  var z = 0;
  for (var i = 0; i < y; i++)
    z = wrap_floor(x + i, false);
  assertEq(z + 10, 2147483661);
}
foo2(0x7ffffff0 + .5, 20);
