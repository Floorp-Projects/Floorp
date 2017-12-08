var BUGNUMBER = 1185106;
var summary = "caller property of function inside async function should return wrapped async function";

print(BUGNUMBER + ": " + summary);

(async function f() {
  var inner = (function g() {
      return g.caller;
  })();
  assertEq(inner, f);
})();

(async function f() {
  "use strict";
  try {
    (function g() {
        return g.caller;
    })();
    assertEq(true, false);
  } catch (e) {
    assertEq(e instanceof TypeError, true);
  }
})();

if (typeof reportCompare === "function")
    reportCompare(true, true);
