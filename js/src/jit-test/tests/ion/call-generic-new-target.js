// Test that the generic call trampoline passes new.target correctly.

function foo(F) {
  return new F();
}

with ({}) {}

class C {
  constructor() {
    this.newTarget = new.target;
  }
}
class C1 extends C {}
class C2 extends C {}
class C3 extends C {}
let C4 = C3.bind({});
let C5 = C3.bind({}, 1);

for (var i = 0; i < 500; i++) {
  assertEq(foo(C1).newTarget, C1);
  assertEq(foo(C2).newTarget, C2);
  assertEq(foo(C3).newTarget, C3);
  assertEq(foo(C4).newTarget, C3);
  assertEq(foo(C5).newTarget, C3);
}
