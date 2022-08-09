setJitCompilerOption("ic.force-megamorphic", 1);

function testBasic() {
  // Create various native objects with dense elements.
  var objs = [[1, 2, 3], {0: 1, 1: 2, 2: 3}];
  var fun = x => x;
  fun[0] = 1;
  fun[1] = 2;
  fun[2] = 3;
  objs.push(fun);
  for (var i = 0; i < 20; i++) {
    var o = {};
    o["x" + i] = i;
    o[0] = 1;
    o[1] = 2;
    if (i < 10) {
      o[2] = 3;
    } else {
      // o[2] will be a hole.
      o[3] = 4;
    }
    objs.push(o);
  }

  // Access their dense elements.
  for (var i = 0; i < 10; i++) {
    for (var j = 0; j < objs.length; j++) {
      var obj = objs[j];
      assertEq(obj[0], 1);
      assertEq(obj[1], 2);
      assertEq(obj[2], j < 13 ? 3 : undefined);
      assertEq(obj[3], j >= 13 ? 4 : undefined);
      assertEq(0 in obj, true);
      assertEq(1 in obj, true);
      assertEq(2 in obj, j < 13);
      assertEq(3 in obj, j >= 13);
      assertEq(Object.hasOwn(obj, 0), true);
      assertEq(Object.hasOwn(obj, 1), true);
      assertEq(Object.hasOwn(obj, 2), j < 13);
      assertEq(Object.hasOwn(obj, 3), j >= 13);
    }
  }
}
testBasic();

function testNonNative() {
  var arr = [1, 2, 3];
  var proxy = new Proxy({}, {
    get(target, prop) { return 456; },
    has(target, prop) { return prop === "0"; },
  });
  for (var i = 0; i < 100; i++) {
    var obj = i < 95 ? arr : proxy;
    assertEq(obj[0], i < 95 ? 1 : 456);
    assertEq(0 in obj, true);
    assertEq(1 in obj, i < 95);
    assertEq(4 in obj, false);
    assertEq(Object.hasOwn(obj, 0), i < 95);
    assertEq(Object.hasOwn(obj, 1), i < 95);
    assertEq(Object.hasOwn(obj, 4), false);
  }
}
testNonNative();
