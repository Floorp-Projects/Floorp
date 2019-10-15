// |jit-test| skip-if: !this.hasOwnProperty("TypedObject")

// Different typed object types to ensure we emit a SetProp IC.
var xs = [
  new new TypedObject.StructType({foo: TypedObject.bigint64}),
  new new TypedObject.StructType({foo: TypedObject.biguint64}),
];

// Store with 0n as rhs.
function storeConstantZero() {
  var value = 0n;

  for (var i = 0; i < 100; ++i) {
    var ta = xs[i & 1];
    ta.foo = value;
  }

  assertEq(xs[0].foo, BigInt.asIntN(64, value));
  assertEq(xs[1].foo, BigInt.asUintN(64, value));
}
storeConstantZero();

// Store non-negative BigInt using inline digits.
function storeInlineDigits() {
  var value = 1n;

  for (var i = 0; i < 100; ++i) {
    var ta = xs[i & 1];
    ta.foo = value;
  }

  assertEq(xs[0].foo, BigInt.asIntN(64, value));
  assertEq(xs[1].foo, BigInt.asUintN(64, value));
}
storeInlineDigits();

// Store negative BigInt using inline digits.
function storeInlineDigitsNegative() {
  var value = -1n;

  for (var i = 0; i < 100; ++i) {
    var ta = xs[i & 1];
    ta.foo = value;
  }

  assertEq(xs[0].foo, BigInt.asIntN(64, value));
  assertEq(xs[1].foo, BigInt.asUintN(64, value));
}
storeInlineDigitsNegative();

// Still inline digits, but now two digits on 32-bit platforms
function storeInlineDigitsTwoDigits() {
  var value = 4294967296n;

  for (var i = 0; i < 100; ++i) {
    var ta = xs[i & 1];
    ta.foo = value;
  }

  assertEq(xs[0].foo, BigInt.asIntN(64, value));
  assertEq(xs[1].foo, BigInt.asUintN(64, value));
}
storeInlineDigitsTwoDigits();

// Negative case of |storeInlineDigitsTwoDigits|.
function storeInlineDigitsTwoDigitsNegative() {
  var value = -4294967296n;

  for (var i = 0; i < 100; ++i) {
    var ta = xs[i & 1];
    ta.foo = value;
  }

  assertEq(xs[0].foo, BigInt.asIntN(64, value));
  assertEq(xs[1].foo, BigInt.asUintN(64, value));
}
storeInlineDigitsTwoDigitsNegative();

// Store BigInt using heap digits.
function storeHeapDigits() {
  var value = 2n ** 1000n;

  for (var i = 0; i < 100; ++i) {
    var ta = xs[i & 1];
    ta.foo = value;
  }

  assertEq(xs[0].foo, BigInt.asIntN(64, value));
  assertEq(xs[1].foo, BigInt.asUintN(64, value));
}
storeHeapDigits();

// Store negative BigInt using heap digits.
function storeHeapDigitsNegative() {
  var value = -(2n ** 1000n);

  for (var i = 0; i < 100; ++i) {
    var ta = xs[i & 1];
    ta.foo = value;
  }

  assertEq(xs[0].foo, BigInt.asIntN(64, value));
  assertEq(xs[1].foo, BigInt.asUintN(64, value));
}
storeHeapDigitsNegative();
