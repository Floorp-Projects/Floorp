var IsCallable = getSelfHostedValue("IsCallable");

setJitCompilerOption("ion.warmup.trigger", 50);

function testSinglePrimitive() {
  var f1 = function() { assertEq(IsCallable(undefined), false); };
  do { f1(); } while (!inIon());

  var f2 = function() { assertEq(IsCallable(null), false); };
  do { f2(); } while (!inIon());

  var f3 = function() { assertEq(IsCallable(true), false); };
  do { f3(); } while (!inIon());

  var f4 = function() { assertEq(IsCallable(1), false); };
  do { f4(); } while (!inIon());

  var f5 = function() { assertEq(IsCallable(1.2), false); };
  do { f5(); } while (!inIon());

  var f6 = function() { assertEq(IsCallable("foo"), false); };
  do { f6(); } while (!inIon());

  var f7 = function() { assertEq(IsCallable(Symbol.iterator), false); };
  do { f7(); } while (!inIon());
}
testSinglePrimitive();

function testMixedPrimitive() {
  var list = [
    undefined,
    null,
    true,
    1,
    1.2,
    "foo",
    Symbol.iterator,
  ];

  var f1 = function() {
    for (let x of list) {
      assertEq(IsCallable(x), false);
    }
  };
  do { f1(); } while (!inIon());
}
testMixedPrimitive();

function testSingleObject() {
  var obj = [];
  var arr = [];

  var f1 = function() { assertEq(IsCallable(obj), false); };
  do { f1(); } while (!inIon());

  var f2 = function() { assertEq(IsCallable(arr), false); };
  do { f2(); } while (!inIon());
}
testSingleObject();

function testMixedPrimitiveAndObject() {
  var list = [
    undefined,
    null,
    true,
    1,
    1.2,
    "foo",
    Symbol.iterator,

    {},
    [],
  ];

  var f1 = function() {
    for (let x of list) {
      assertEq(IsCallable(x), false);
    }
  };
  do { f1(); } while (!inIon());
}
testMixedPrimitiveAndObject();

function testFunction() {
  var f1 = function() { assertEq(IsCallable(Function), true); };
  do { f1(); } while (!inIon());

  var f2 = function() { assertEq(IsCallable(parseInt), true); };
  do { f2(); } while (!inIon());
}
testFunction();

function testProxy() {
  var p1 = new Proxy({}, {});
  var f1 = function() { assertEq(IsCallable(p1), false); };
  do { f1(); } while (!inIon());

  var p2 = new Proxy(function() {}, {});
  var f2 = function() { assertEq(IsCallable(p2), true); };
  do { f2(); } while (!inIon());
}
testProxy();

function testMixed() {
  var p1 = new Proxy({}, {});
  var p2 = new Proxy(function() {}, {});

  var list = [
    [undefined, false],
    [null, false],
    [true, false],
    [1, false],
    [1.2, false],
    ["foo", false],
    [Symbol.iterator, false],

    [{}, false],
    [[], false],

    [Function, true],
    [parseInt, true],

    [p1, false],
    [p2, true],
  ];

  var f1 = function() {
    for (let [x, expected] of list) {
      assertEq(IsCallable(x), expected);
    }
  };
  do { f1(); } while (!inIon());
}
testMixed();
