// The shape teleporting optimization can be disabled for an object that's used
// as prototype when either it's involved in prototype changes or it had a property
// shadowed on another prototype object.

function changeProps(o) {
  Object.assign(o, {x: 1, y: 2, z: 3});
  o.foo = 4;
  delete o.x;
}

function testProtoChange() {
  var receiver = {};
  var A = Object.create(null);
  var B = Object.create(A);

  // Change |receiver|'s proto: receiver => B => A => null
  // Because |receiver| is not used as a prototype object, this doesn't affect
  // teleporting.
  Object.setPrototypeOf(receiver, B);
  assertEq(hasInvalidatedTeleporting(receiver), false);
  assertEq(hasInvalidatedTeleporting(A), false);
  assertEq(hasInvalidatedTeleporting(B), false);

  // Change B's proto to C: receiver => B => C => null
  // Because B is used as prototype object, both A and B invalidate teleporting.
  var C = Object.create(null);
  Object.setPrototypeOf(B, C);
  assertEq(hasInvalidatedTeleporting(receiver), false);
  assertEq(hasInvalidatedTeleporting(A), true);
  assertEq(hasInvalidatedTeleporting(B), true);
  assertEq(hasInvalidatedTeleporting(C), false);

  // Change B's proto a second time: receiver => B => D => null
  // Now C has teleporting invalidated too.
  var D = Object.create(null);
  Object.setPrototypeOf(B, D);
  assertEq(hasInvalidatedTeleporting(receiver), false);
  assertEq(hasInvalidatedTeleporting(A), true);
  assertEq(hasInvalidatedTeleporting(B), true);
  assertEq(hasInvalidatedTeleporting(C), true);
  assertEq(hasInvalidatedTeleporting(D), false);

  // Changing properties (without shadowing) must not affect teleporting state.
  changeProps(C);
  changeProps(D);
  assertEq(hasInvalidatedTeleporting(C), true);
  assertEq(hasInvalidatedTeleporting(D), false);
}
testProtoChange();

function testShadowingProp() {
  // receiver => C => B => A => null
  var A = Object.create(null);
  var B = Object.create(A);
  var C = Object.create(B);
  var receiver = Object.create(C);

  // Adding non-shadowing properties doesn't affect teleporting.
  A.a = 1;
  B.b = 1;
  C.c = 1;
  receiver.receiver = 1;
  assertEq(hasInvalidatedTeleporting(receiver), false);
  assertEq(hasInvalidatedTeleporting(C), false);
  assertEq(hasInvalidatedTeleporting(B), false);
  assertEq(hasInvalidatedTeleporting(A), false);

  // Objects not used as prototype can shadow properties without affecting
  // teleporting.
  receiver.a = 1;
  receiver.b = 2;
  receiver.c = 3;
  assertEq(hasInvalidatedTeleporting(receiver), false);
  assertEq(hasInvalidatedTeleporting(C), false);
  assertEq(hasInvalidatedTeleporting(B), false);
  assertEq(hasInvalidatedTeleporting(A), false);

  // Shadowing a property of B on C invalidates teleporting for B.
  C.b = 1;
  assertEq(hasInvalidatedTeleporting(receiver), false);
  assertEq(hasInvalidatedTeleporting(C), false);
  assertEq(hasInvalidatedTeleporting(B), true);
  assertEq(hasInvalidatedTeleporting(A), false);

  // Shadowing a property of A on C invalidates teleporting for A.
  C.a = 2;
  assertEq(hasInvalidatedTeleporting(receiver), false);
  assertEq(hasInvalidatedTeleporting(C), false);
  assertEq(hasInvalidatedTeleporting(B), true);
  assertEq(hasInvalidatedTeleporting(A), true);

  // Changing properties (without shadowing) must not affect teleporting state.
  changeProps(C);
  changeProps(B);
  assertEq(hasInvalidatedTeleporting(C), false);
  assertEq(hasInvalidatedTeleporting(B), true);
}
testShadowingProp();

function testShadowingPropStopsAtFirst() {
  // receiver => C => B{x,y} => A{x,y,z} => null
  var A = Object.create(null);
  A.x = 1;
  A.y = 2;
  A.z = 3;
  var B = Object.create(A);
  B.x = 1;
  B.y = 2;
  var C = Object.create(B);
  var receiver = Object.create(C);

  // Teleporting is supported.
  assertEq(hasInvalidatedTeleporting(receiver), false);
  assertEq(hasInvalidatedTeleporting(C), false);
  assertEq(hasInvalidatedTeleporting(B), false);
  assertEq(hasInvalidatedTeleporting(A), false);

  // Shadowing a property of B (and A) on C.
  // This invalidates teleporting for B but not for A, because the search stops
  // at B.
  C.x = 1;
  assertEq(hasInvalidatedTeleporting(receiver), false);
  assertEq(hasInvalidatedTeleporting(C), false);
  assertEq(hasInvalidatedTeleporting(B), true);
  assertEq(hasInvalidatedTeleporting(A), false);

  // "y" is similar.
  C.y = 2;
  assertEq(hasInvalidatedTeleporting(receiver), false);
  assertEq(hasInvalidatedTeleporting(C), false);
  assertEq(hasInvalidatedTeleporting(B), true);
  assertEq(hasInvalidatedTeleporting(A), false);

  // "z" is only defined on A, so now A is affected.
  C.z = 3;
  assertEq(hasInvalidatedTeleporting(receiver), false);
  assertEq(hasInvalidatedTeleporting(C), false);
  assertEq(hasInvalidatedTeleporting(B), true);
  assertEq(hasInvalidatedTeleporting(A), true);
}
testShadowingPropStopsAtFirst();

// Ensure teleporting properties on Object.prototype is still possible.
assertEq(hasInvalidatedTeleporting(Object.prototype), false);
