function g() {
      return g.caller;
}

(async function f() {
  var inner = g();
  assertEq(inner, null);
})();

(async function f() {
  "use strict";
  var inner = g();
  assertEq(inner, null);
})();

if (typeof reportCompare === "function")
    reportCompare(true, true);
