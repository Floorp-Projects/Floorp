setJitCompilerOption("ic.force-megamorphic", 1);

function testAddToIntermediate() {
  var C = Object.create(null);
  var B = Object.create(C);
  var A = Object.create(B);
  C.x = 1;
  for (var i = 0; i < 50; i++) {
    assertEq(A.x, i <= 45 ? 1 : 0);
    if (i === 45) {
      B.x = 0;
    }
  }
}
for (var i = 0; i < 5; i++) {
  testAddToIntermediate();
}

function testAddToReceiver() {
  var C = Object.create(null);
  var B = Object.create(C);
  var A = Object.create(B);
  C.x = 1;
  for (var i = 0; i < 50; i++) {
    assertEq(A.x, i <= 45 ? 1 : 0);
    if (i === 45) {
      A.x = 0;
    }
  }
}
for (var i = 0; i < 5; i++) {
  testAddToReceiver();
}

function testDelete() {
  var C = Object.create(null);
  var B = Object.create(C);
  var A = Object.create(B);
  C.x = 1;
  for (var i = 0; i < 50; i++) {
    assertEq(A.x, i <= 45 ? 1 : undefined);
    if (i === 45) {
      delete C.x;
    }
  }
}
for (var i = 0; i < 5; i++) {
  testDelete();
}

function testDataToAccessor() {
  var C = Object.create(null);
  var B = Object.create(C);
  var A = Object.create(B);
  C.x = 1;
  for (var i = 0; i < 50; i++) {
    assertEq(A.x, i <= 45 ? 1 : 7);
    if (i === 45) {
      Object.defineProperty(C, "x", {get: () => 7});
    }
  }
}
for (var i = 0; i < 5; i++) {
  testDataToAccessor();
}

function testProtoChange() {
  var C = Object.create(null);
  var B = Object.create(C);
  var A = Object.create(B);
  C.x = 1;
  for (var i = 0; i < 50; i++) {
    assertEq(A.x, i <= 45 ? 1 : 8);
    if (i === 45) {
      Object.setPrototypeOf(B, {a: 0, b: 1, x: 8});
    }
  }
}
for (var i = 0; i < 5; i++) {
  testProtoChange();
}
