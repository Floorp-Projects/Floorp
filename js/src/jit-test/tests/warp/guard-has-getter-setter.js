// Access property once.
function simple() {
  var obj = {
    get p() {
      return 1;
    }
  };

  // Use objects with different shapes to enter megamorphic state for
  // the JSOp::GetProp opcode.
  var array = [
    Object.create(obj, {a: {value: 1}}),
    Object.create(obj, {b: {value: 2}}),
    Object.create(obj, {c: {value: 3}}),
    Object.create(obj, {d: {value: 4}}),
    Object.create(obj, {e: {value: 5}}),
    Object.create(obj, {f: {value: 6}}),
    Object.create(obj, {g: {value: 7}}),
    Object.create(obj, {h: {value: 8}}),
  ];

  var r = 0;
  for (var i = 0; i < 200; ++i) {
    var o = array[i & 7];
    r += o.p;
  }
  assertEq(r, 200);
}
simple();

// Access property multiple times (consecutive) to test that MGuardHasGetterSetter
// ops can be merged.
function consecutive() {
  var obj = {
    get p() {
      return 1;
    }
  };

  // Use objects with different shapes to enter megamorphic state for
  // the JSOp::GetProp opcode.
  var array = [
    Object.create(obj, {a: {value: 1}}),
    Object.create(obj, {b: {value: 2}}),
    Object.create(obj, {c: {value: 3}}),
    Object.create(obj, {d: {value: 4}}),
    Object.create(obj, {e: {value: 5}}),
    Object.create(obj, {f: {value: 6}}),
    Object.create(obj, {g: {value: 7}}),
    Object.create(obj, {h: {value: 8}}),
  ];

  var r = 0;
  for (var i = 0; i < 200; ++i) {
    var o = array[i & 7];

    r += o.p;
    r += o.p;
    r += o.p;
    r += o.p;
  }
  assertEq(r, 4 * 200);
}
consecutive();

// Access property multiple times (loop) to test LICM.
function loop() {
  var obj = {
    get p() {
      return 1;
    }
  };

  // Use objects with different shapes to enter megamorphic state for
  // the JSOp::GetProp opcode.
  var array = [
    Object.create(obj, {a: {value: 1}}),
    Object.create(obj, {b: {value: 2}}),
    Object.create(obj, {c: {value: 3}}),
    Object.create(obj, {d: {value: 4}}),
    Object.create(obj, {e: {value: 5}}),
    Object.create(obj, {f: {value: 6}}),
    Object.create(obj, {g: {value: 7}}),
    Object.create(obj, {h: {value: 8}}),
  ];

  var r = 0;
  for (var i = 0; i < 200; ++i) {
    var o = array[i & 7];

    for (var j = 0; j < 5; ++j) {
      r += o.p;
    }
  }
  assertEq(r, 5 * 200);
}
loop();

// Bailout when prototype changes.
function modifyProto() {
  var obj = {
    get p() {
      return 1;
    }
  };

  var obj2 = {
    get p() {
      return 2;
    }
  };

  // Use objects with different shapes to enter megamorphic state for
  // the JSOp::GetProp opcode.
  var array = [
    Object.create(obj, {a: {value: 1}}),
    Object.create(obj, {b: {value: 2}}),
    Object.create(obj, {c: {value: 3}}),
    Object.create(obj, {d: {value: 4}}),
    Object.create(obj, {e: {value: 5}}),
    Object.create(obj, {f: {value: 6}}),
    Object.create(obj, {g: {value: 7}}),
    Object.create(obj, {h: {value: 8}}),
  ];

  var r = 0;
  for (var i = 0; i < 200; ++i) {
    var o = array[i & 7];

    r += o.p;

    // Always execute Object.setPrototypeOf() to avoid cold code bailouts,
    // which would happen for conditional code like if-statements. But only
    // actually change |o|'s prototype once.
    var j = (i === 100) | 0;
    var q = [{}, o][j];
    Object.setPrototypeOf(q, obj2);

    r += o.p;
  }
  assertEq(r, 2 * 200 + Math.floor(100 / 8) * 2 + 1);
}
modifyProto();

// Bailout when property is changed to own data property.
function modifyToOwnValue() {
  var obj = {
    get p() {
      return 1;
    }
  };

  // Use objects with different shapes to enter megamorphic state for
  // the JSOp::GetProp opcode.
  var array = [
    Object.create(obj, {a: {value: 1}}),
    Object.create(obj, {b: {value: 2}}),
    Object.create(obj, {c: {value: 3}}),
    Object.create(obj, {d: {value: 4}}),
    Object.create(obj, {e: {value: 5}}),
    Object.create(obj, {f: {value: 6}}),
    Object.create(obj, {g: {value: 7}}),
    Object.create(obj, {h: {value: 8}}),
  ];

  var r = 0;
  for (var i = 0; i < 200; ++i) {
    var o = array[i & 7];

    r += o.p;

    // Always execute Object.setPrototypeOf() to avoid cold code bailouts,
    // which would happen for conditional code like if-statements. But only
    // actually change |o|'s prototype once.
    var j = (i === 100) | 0;
    var q = [{}, o][j];
    Object.defineProperty(q, "p", {value: 2});

    r += o.p;
  }
  assertEq(r, 2 * 200 + Math.floor(100 / 8) * 2 + 1);
}
modifyToOwnValue();

// Bailout when property is changed to own accessor property.
function modifyToOwnAccessor() {
  var obj = {
    get p() {
      return 1;
    }
  };

  // Use objects with different shapes to enter megamorphic state for
  // the JSOp::GetProp opcode.
  var array = [
    Object.create(obj, {a: {value: 1}}),
    Object.create(obj, {b: {value: 2}}),
    Object.create(obj, {c: {value: 3}}),
    Object.create(obj, {d: {value: 4}}),
    Object.create(obj, {e: {value: 5}}),
    Object.create(obj, {f: {value: 6}}),
    Object.create(obj, {g: {value: 7}}),
    Object.create(obj, {h: {value: 8}}),
  ];

  var r = 0;
  for (var i = 0; i < 200; ++i) {
    var o = array[i & 7];

    r += o.p;

    // Always execute Object.setPrototypeOf() to avoid cold code bailouts,
    // which would happen for conditional code like if-statements. But only
    // actually change |o|'s prototype once.
    var j = (i === 100) | 0;
    var q = [{}, o][j];
    Object.defineProperty(q, "p", {get() { return 2; }});

    r += o.p;
  }
  assertEq(r, 2 * 200 + Math.floor(100 / 8) * 2 + 1);
}
modifyToOwnAccessor();

// Bailout when changing accessor.
function modifyProtoAccessor() {
  var obj = {
    get p() {
      return 1;
    }
  };

  // Use objects with different shapes to enter megamorphic state for
  // the JSOp::GetProp opcode.
  var array = [
    Object.create(obj, {a: {value: 1}}),
    Object.create(obj, {b: {value: 2}}),
    Object.create(obj, {c: {value: 3}}),
    Object.create(obj, {d: {value: 4}}),
    Object.create(obj, {e: {value: 5}}),
    Object.create(obj, {f: {value: 6}}),
    Object.create(obj, {g: {value: 7}}),
    Object.create(obj, {h: {value: 8}}),
  ];

  var r = 0;
  for (var i = 0; i < 200; ++i) {
    var o = array[i & 7];

    r += o.p;

    // Always execute Object.setPrototypeOf() to avoid cold code bailouts,
    // which would happen for conditional code like if-statements. But only
    // actually change |o|'s prototype once.
    var j = (i === 100) | 0;
    var q = [{}, obj][j];
    Object.defineProperty(q, "p", {get() { return 2; }});

    r += o.p;
  }
  assertEq(r, 2 * 200 + 100 * 2 - 1);
}
modifyProtoAccessor();
