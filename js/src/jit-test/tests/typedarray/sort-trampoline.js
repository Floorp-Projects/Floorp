function testGC() {
  for (var i = 0; i < 20; i++) {
    var arr = new BigInt64Array([1n, 3n, 0n, 12345678901234n, -12345678901234567n]);
    arr.sort((x, y) => {
      if (i === 17) {
        gc();
      }
      return Number(x) - Number(y);
    });
    assertEq(arr.join(), "-12345678901234567,0,1,3,12345678901234");
  }
}
testGC();

function testDetach() {
  for (var i = 0; i < 20; i++) {
    var arr = new Float32Array([1, 2.5, -0, 3]);
    arr.sort((x, y) => {
      if (i > 15) {
        detachArrayBuffer(arr.buffer);
      }
      return x - y;
    });
    if (i > 15) {
      assertEq(arr.length, 0);
      assertEq(arr[0], undefined);
    } else {
      assertEq(arr.join(","), "0,1,2.5,3");
    }
  }
}
testDetach();

function testException() {
  var arr = new BigInt64Array([1n, 3n, 0n, 4n, 1n]);
  var ex;
  try {
    for (var i = 0; i < 20; i++) {
      arr.sort((x, y) => {
        if (i === 17) {
          throw "fit";
        }
        return Number(x) - Number(y);
      });
    }
  } catch (e) {
    ex = e;
  }
  assertEq(ex, "fit");
  assertEq(i, 17);
  assertEq(arr.join(), "0,1,1,3,4");

}
testException();

function testRectifier() {
  var arr = new Uint32Array([0xffff, 0xffff_ffff, -1, 0]);
  for (var i = 0; i < 20; i++) {
    arr.sort(function(x, y, a) {
      assertEq(arguments.length, 2);
      assertEq(a, undefined);
      return y - x;
    });
  }
  assertEq(arr.join(), "4294967295,4294967295,65535,0");
}
testRectifier();

function testClassConstructor() {
  var normal = (x, y) => x - y;
  var dummy = {};
  var ctor = (class { constructor(x, y) {
    assertEq(x, dummy);
  }});
  // Warm up the constructor.
  for (var i = 0; i < 20; i++) {
    new ctor(dummy, dummy);
  }
  for (var i = 0; i < 20; i++) {
    var arr = new Uint8Array([0, 5, 1, 3]);
    var ex;
    try {
      arr.sort(i < 17 ? normal : ctor);
    } catch (e) {
      ex = e;
    }
    assertEq(ex instanceof TypeError, i >= 17);
    assertEq(arr.join(""), i >= 17 ? "0513" : "0135");
  }
}
testClassConstructor();

function testSwitchRealms() {
  var arr = new Uint8ClampedArray([5, 3, 0, 1]);
  var g = newGlobal({sameCompartmentAs: this});
  g.foo = 123;
  var comp = g.evaluate(`((x, y) => {
    assertEq(foo, 123);
    return x - y;
  })`);
  for (var i = 0; i < 20; i++) {
    arr.sort(comp);
  }
  assertEq(arr.join(""), "0135");
}
testSwitchRealms();

function testCrossCompartment() {
  var g = newGlobal({newCompartment: true});
  var wrapper = g.evaluate(`((x, y) => {
    return Number(x) - Number(y);
  })`);
  for (var i = 0; i < 20; i++) {
    var arr = new BigUint64Array([1n, 0n, 1234567890123n, 98765432109876n, 0n]);
    arr.sort(wrapper);
    assertEq(arr.join(), "0,0,1,1234567890123,98765432109876");
  }
}
testCrossCompartment();

function testBound() {
  var fun = (function(a, x, y) {
    "use strict";
    assertEq(this, null);
    assertEq(a, 1);
    return Number(x) - Number(y);
  }).bind(null, 1);
  for (var i = 0; i < 20; i++) {
    var arr = new BigUint64Array([1n, 0n, 1234567890123n, 98765432109876n, 0n]);
    arr.sort(fun);
    assertEq(arr.join(), "0,0,1,1234567890123,98765432109876");
  }
}
testBound();

function testExtraArgs() {
  var arr = new Int8Array([1, 3, 5, 0]);
  var cmp = (x, y) => x - y - 1;
  for (var i = 0; i < 20; i++) {
    arr.sort(cmp, cmp, cmp, cmp, cmp, cmp, cmp);
  }
  assertEq(arr.join(""), "1035");
}
testExtraArgs();

function testBailout() {
  for (var i = 0; i < 110; i++) {
    var arr = new Float64Array([1, 3.3, 5.5, -0, NaN]);
    arr.sort(function(x, y) {
      if (i === 108) {
        bailout();
      }
      return x - y;
    });
    assertEq(arr.join(","), "0,1,3.3,5.5,NaN");
  }
}
testBailout();

function testExceptionHandlerSwitchRealm() {
  var g = newGlobal({sameCompartmentAs: this});
  for (var i = 0; i < 25; i++) {
    var ex = null;
    try {
      Int8Array.prototype.toSorted.call(new Int8Array([1, 2, 3]), () => {
        throw "fit";
      });
    } catch (e) {
      ex = e;
    }
    assertEq(ex, "fit");
  }
}
testExceptionHandlerSwitchRealm();
