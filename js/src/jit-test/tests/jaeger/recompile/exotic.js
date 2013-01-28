
// Test exotic ways of triggering recompilation.

// Lowered native call.

var x = 0;
var y = true;
for (var i = 0; i < 20; i++) {
  x += Array.map.apply(undefined, [[0], function(x) { if (i == 10) eval("y = 20"); return 1; }])[0];
}
assertEq(x, 20);
assertEq(y, 20);

// Recompilation triggered by local function.

var o = {};
function what(q) {
  function inner() { return q; }
  o.f = inner;
  var a = o.f();
  return a;
}
for (var i = 0; i < 10; i++) {
  var a = what(i);
  assertEq(a, i);
}

// Lowered scripted call to apply returning code pointer.

var global = 3;
function foo(x, y) {
  var q = x.apply(null, y);
  if (q != 10)
    assertEq(global, true);
}
foo(function(a) { global = a; return 10; }, [1]);
foo(function(a) { global = a; return 10; }, [1]);
foo(function(a) { global = a; return 10; }, [1]);
assertEq(global, 1);
foo(function(a) { global = a; return 3; }, [true]);
assertEq(global, true);

// Lowered scripted call returning NULL.

var oglobal = 3;
function xfoo(x, y) {
  var q = x.apply(null, y);
  if (q != 10)
    assertEq(oglobal, true);
}
xfoo(function(a) { oglobal = a; return 10; }, [1]);
xfoo(function(a) { oglobal = a; return 10; }, [1]);
xfoo(function(a) { oglobal = a; return 10; }, [1]);
assertEq(oglobal, 1);
xfoo(function(a) { [1,2,3]; oglobal = a; return 3; }, [true]);
assertEq(oglobal, true);

// Recompilation out of SplatApplyArgs.

weirdarray = [,,1,2,3];
Object.defineProperty(weirdarray, 0, {get: function() { vglobal = 'true'; }});

var vglobal = 3;
function yfoo(x, y) {
  var q = x.apply(null, y);
  if (q != 10)
    assertEq(vglobal, 'true');
  else
    assertEq(vglobal, 3);
}
yfoo(function(a) { return 10; }, [1]);
yfoo(function(a) { return 10; }, [1]);
yfoo(function(a) { return 10; }, [1]);
yfoo(function() { return 0; }, weirdarray);
