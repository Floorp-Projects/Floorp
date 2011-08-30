
/* Recompilation while being processed by property ICs. */

var ga = 10;
var gb = 10;

Object.defineProperty(Object.prototype, "a", {
    set: function(a) { eval("ga = true;"); },
    get: function() { eval("gb = true;"); }
  });

function foo() {
  var x = {};
  x.a = 10;
  assertEq(ga + 1, 2);
}
foo();

function bar() {
  var x = {};
  var a = x.a;
  assertEq(gb + 1, 2);
}
bar();
