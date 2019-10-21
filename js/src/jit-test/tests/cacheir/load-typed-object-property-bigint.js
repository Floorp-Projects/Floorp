// |jit-test| skip-if: !this.hasOwnProperty("TypedObject")

// Different typed object types to ensure we emit a GetProp IC.
var xs = [
  new new TypedObject.StructType({foo: TypedObject.bigint64}),
  new new TypedObject.StructType({foo: TypedObject.biguint64}),
];

// Load 0n value.
function loadConstantZero() {
  var value = 0n;

  xs[0].foo = value;
  xs[1].foo = value;

  var ys = [
    BigInt.asIntN(64, value),
    BigInt.asUintN(64, value),
  ];

  for (var i = 0; i < 100; ++i) {
    var ta = xs[i & 1];
    assertEq(ta.foo, ys[i & 1]);
  }
}
loadConstantZero();

// Load non-negative BigInt using inline digits.
function loadInlineDigits() {
  var value = 1n;

  xs[0].foo = value;
  xs[1].foo = value;

  var ys = [
    BigInt.asIntN(64, value),
    BigInt.asUintN(64, value),
  ];

  for (var i = 0; i < 100; ++i) {
    var ta = xs[i & 1];
    assertEq(ta.foo, ys[i & 1]);
  }
}
loadInlineDigits();

// Load negative BigInt using inline digits.
function loadInlineDigitsNegative() {
  var value = -1n;

  xs[0].foo = value;
  xs[1].foo = value;

  var ys = [
    BigInt.asIntN(64, value),
    BigInt.asUintN(64, value),
  ];

  for (var i = 0; i < 100; ++i) {
    var ta = xs[i & 1];
    assertEq(ta.foo, ys[i & 1]);
  }
}
loadInlineDigitsNegative();

// Still inline digits, but now two digits on 32-bit platforms
function loadInlineDigitsTwoDigits() {
  var value = 4294967296n;

  xs[0].foo = value;
  xs[1].foo = value;

  var ys = [
    BigInt.asIntN(64, value),
    BigInt.asUintN(64, value),
  ];

  for (var i = 0; i < 100; ++i) {
    var ta = xs[i & 1];
    assertEq(ta.foo, ys[i & 1]);
  }
}
loadInlineDigitsTwoDigits();

// Negative case of |storeInlineDigitsTwoDigits|.
function loadInlineDigitsTwoDigitsNegative() {
  var value = -4294967296n;

  xs[0].foo = value;
  xs[1].foo = value;

  var ys = [
    BigInt.asIntN(64, value),
    BigInt.asUintN(64, value),
  ];

  for (var i = 0; i < 100; ++i) {
    var ta = xs[i & 1];
    assertEq(ta.foo, ys[i & 1]);
  }
}
loadInlineDigitsTwoDigitsNegative();
