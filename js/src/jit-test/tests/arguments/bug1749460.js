function opaque(x) {
  with ({}) {}
  return x;
}

function foo() {
  "use strict";
  Object.defineProperty(arguments, 0, {value: 1});
  return opaque(...arguments);
}

for (var i = 0; i < 50; i++) {
  assertEq(foo(0), 1);
}
