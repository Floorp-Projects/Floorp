// Test that callees that resolve to bindings on the global object or the
// global lexical environment get an 'undefined' this inside with scopes.

let g = function () { "use strict"; assertEq(this, undefined); }
function f() { "use strict"; assertEq(this, undefined); }

with ({}) { 
  // f is resolved on the global object
  f();
  // g is resolved on the global lexical environment
  g();
}

f();
g();

if (typeof reportCompare === "function")
  reportCompare(true, true);
